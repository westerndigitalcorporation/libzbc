/*
 * This file is part of libzbc.
 * 
 * Copyright (C) 2009-2014, HGST, Inc.  This software is distributed
 * under the terms of the GNU Lesser General Public License version 3,
 * or any later version, "as is," without technical support, and WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  You should have received a copy
 * of the GNU Lesser General Public License along with libzbc.  If not,
 * see <http://www.gnu.org/licenses/>.
 * 
 * Authors: Damien Le Moal (damien.lemoal@hgst.com)
 *          Christophe Louargant (christophe.louargant@hgst.com)
 */

/***** Including files *****/

#define _GNU_SOURCE     /* O_LARGEFILE */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <libzbc/zbc.h>

/***** Local functions *****/

/**
 * I/O abort.
 */
static int zbc_write_zone_abort = 0;

/**
 * System time in usecs.
 */
static __inline__ unsigned long long
zbc_write_zone_usec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return( (unsigned long long) tv.tv_sec * 1000000LL + (unsigned long long) tv.tv_usec );

}

/**
 * Signal handler.
 */
static void
zbc_write_zone_sigcatcher(int sig)
{

    zbc_write_zone_abort = 1;

    return;

}

/***** Main *****/

int main(int argc,
         char **argv)
{
    struct zbc_device_info info;
    struct zbc_device *dev = NULL;
    unsigned long long elapsed;
    unsigned long long bcount = 0;
    unsigned long long fsize, brate;
    struct stat st;
    int zidx;
    int floop = 0, fd = -1, i, ret = 1;
    size_t iosize, ios;
    void *iobuf = NULL;
    uint32_t lba_count;
    int ionum = 0;
    struct zbc_zone *zones = NULL;
    struct zbc_zone *iozone = NULL;
    unsigned int nr_zones, iocount = 0;
    char *path, *file = NULL;
    long long lba_ofst = -1;

    /* Check command line */
    if ( argc < 4 ) {
usage:
        printf("Usage: %s [options] <dev> <zone no> <I/O size>\n"
               "  Write into a zone from the current write pointer until\n"
               "  the zone is full or the number of I/O specified is executed\n"
               "Options:\n"
               "    -v         : Verbose mode\n"
               "    -nio <num> : Limit the number of I/O executed to <num>\n"
               "    -f <file>  : Write the content of <file>\n"
               "    -loop      : If a file is specified, repeatedly write the\n"
               "                 file to the zone until the zone is full.\n"
               "    -lba       : lba offset, from given zone <zone no> starting lba, where to write.\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 1); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( strcmp(argv[i], "-nio") == 0 ) {

            if ( i >= (argc - 1) ) {
                goto usage;
            }
            i++;

            ionum = atoi(argv[i]);
            if ( ionum <= 0 ) {
                fprintf(stderr, "Invalid number of I/Os\n");
                return( 1 );
            }

        } else if ( strcmp(argv[i], "-f") == 0 ) {

            if ( i >= (argc - 1) ) {
                goto usage;
            }
            i++;

            file = argv[i];

        } else if ( strcmp(argv[i], "-loop") == 0 ) {

            floop = 1;

        } else if ( strcmp(argv[i], "-lba") == 0 ) {

            if ( i >= (argc - 1) ) {
                goto usage;
            }
            i++;

            lba_ofst = atoll(argv[i]);
            if ( lba_ofst < 0 ) {
                fprintf(stderr, "LBA offset has to be greater or equal to 0.\n");
                return( 1 );
            }

        } else if ( argv[i][0] == '-' ) {

            fprintf(stderr,
                    "Unknown option \"%s\"\n",
                    argv[i]);
            goto usage;

        } else {

            break;

        }

    }

    if ( i != (argc - 3) ) {
        goto usage;
    }

    /* Get parameters */
    path = argv[i];
    zidx = atoi(argv[i + 1]);
    iosize = atoi(argv[i + 2]);

    /* Setup signal handler */
    signal(SIGQUIT, zbc_write_zone_sigcatcher);
    signal(SIGINT, zbc_write_zone_sigcatcher);
    signal(SIGTERM, zbc_write_zone_sigcatcher);

    /* Open device */
    ret = zbc_open(path, O_WRONLY, &dev);
    if ( ret != 0 ) {
        return( 1 );
    }

    ret = zbc_get_device_info(dev, &info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_get_device_info failed\n");
        goto out;
    }

    /* Get zone list */
    ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
    if ( ret != 0 ) {
        fprintf(stderr, "zbc_list_zones failed\n");
        ret = 1;
        goto out;
    }

    /* Get target zone */
    if ( zidx >= nr_zones ) {
        fprintf(stderr, "Target zone not found\n");
        ret = 1;
        goto out;
    }
    iozone = &zones[zidx];

    printf("Device %s: %s interface, %s disk model\n",
           path,
           zbc_disk_type_str(info.zbd_type),
           zbc_disk_model_str(info.zbd_model));
    printf("    %llu logical blocks of %u B\n",
           (unsigned long long) info.zbd_logical_blocks,
           (unsigned int) info.zbd_logical_block_size);
    printf("    %llu physical blocks of %u B\n",
           (unsigned long long) info.zbd_physical_blocks,
           (unsigned int) info.zbd_physical_block_size);

    printf("Target zone: Zone %d / %d, type 0x%x, cond 0x%x, need_reset %d, non_seq %d, LBA %11llu, %11llu sectors, wp %11llu\n",
           zidx,
           nr_zones,
           iozone->zbz_type,
           iozone->zbz_condition,
           iozone->zbz_need_reset,
           iozone->zbz_non_seq,
           zbc_zone_start_lba(iozone),
           zbc_zone_length(iozone),
           zbc_zone_wp_lba(iozone));

    /* Get an I/O buffer */
    if ( (! iosize)
         || (iosize % info.zbd_physical_block_size) ) {
        fprintf(stderr,
                "Invalid I/O size %zu (must be aligned on %u)\n",
                iosize,
                (unsigned int) info.zbd_physical_block_size);
        ret = 1;
        goto out;
    }
    ret = posix_memalign((void **) &iobuf, info.zbd_physical_block_size, iosize);
    if ( ret != 0 ) {
        fprintf(stderr,
                "No memory for I/O buffer (%zu B)\n",
                iosize);
        ret = 1;
        goto out;
    }

    /* Open the file to write, if any */
    if ( file ) {

        fd = open(file, O_LARGEFILE | O_RDONLY);
        if ( fd < 0 ) {
            fprintf(stderr, "Open file \"%s\" failed %d (%s)\n",
                    file,
                    errno,
                    strerror(errno));
            ret = 1;
            goto out;
        }

        ret = fstat(fd, &st);
        if ( ret != 0 ) {
            fprintf(stderr, "Stat file \"%s\" failed %d (%s)\n",
                    file,
                    errno,
                    strerror(errno));
            ret = 1;
            goto out;
        }

        if ( S_ISREG(st.st_mode) ) {
            fsize = st.st_size;
        } else if ( S_ISBLK(st.st_mode) ) {
            ret = ioctl(fd, BLKGETSIZE64, &fsize);
            if ( ret != 0 ) {
                fprintf(stderr,
                        "ioctl BLKGETSIZE64 block device \"%s\" failed %d (%s)\n",
                        file,
                        errno,
                        strerror(errno));
                ret = 1;
                goto out;
            }
        } else {
            fprintf(stderr, "Unsupported file \"%s\" type\n",
                    file);
            ret = 1;
            goto out;
        }

        printf("Writting file \"%s\" (%llu B) to target zone %d, %zu B I/Os\n",
               file,
               fsize,
               zidx,
               iosize);

    } else if ( ! ionum ) {

        printf("Filling target zone %d, %zu B I/Os\n",
               zidx,
               iosize);

    } else {

        printf("Writting to target zone %d, %d I/Os of %zu B\n",
               zidx,
               ionum,
               iosize);

    }

    elapsed = zbc_write_zone_usec();

    while( (! zbc_write_zone_abort)
           && (zbc_zone_wp_lba(iozone) < zbc_zone_end_lba(iozone)) ) {

        lba_count = iosize / info.zbd_physical_block_size;

        if ( file ) {

            /* Read file */
            ret = read(fd, iobuf, iosize);
            if ( ret < 0 ) {
                fprintf(stderr, "Read file \"%s\" failed %d (%s)\n",
                        file,
                        errno,
                        strerror(errno));
                ret = 1;
                break;
            }

            ios = ret;
            if ( ios < iosize ) {
                if ( floop ) {
                    /* Rewind and read remaining of buffer */
                    lseek(fd, 0, SEEK_SET);
                    ret = read(fd, iobuf + ios, iosize - ios);
                    if ( ret < 0 ) {
                        fprintf(stderr, "Read file \"%s\" failed %d (%s)\n",
                                file,
                                errno,
                                strerror(errno));
                        ret = 1;
                        break;
                    }
                    ios += ret;
                } else if ( ios ) {
                    /* Clear end of buffer */
                    memset(iobuf + ios, 0, iosize - ios);
                }
            }

            if ( ! ios ) {
                /* EOF */
                break;
            }

            lba_count = ios / info.zbd_physical_block_size;

        }

        /* Write to zone */
        if ( (zbc_zone_wp_lba(iozone) + lba_count) > zbc_zone_end_lba(iozone) ) {
            lba_count = zbc_zone_end_lba(iozone) - zbc_zone_wp_lba(iozone);
        }

        if ( lba_ofst >= 0 ) {
            ret = zbc_pwrite(dev, iozone, iobuf, lba_count, lba_ofst);
        } else {
            ret = zbc_write(dev, iozone, iobuf, lba_count);
        }
        if ( ret <= 0 ) {
            ret = 1;
            break;
        }

        bcount += lba_count * info.zbd_physical_block_size;
        iocount++;

        if ( (ionum > 0) && (iocount >= ionum) ) {
            break;
        }

    }

    elapsed = zbc_write_zone_usec() - elapsed;

    if ( elapsed ) {
        printf("Wrote %llu B (%d I/Os) in %llu.%03llu sec\n",
               bcount,
               iocount,
               elapsed / 1000000,
               (elapsed % 1000000) / 1000);
        printf("  IOPS %llu)\n",
               iocount * 1000000 / elapsed);
        brate = bcount * 1000000 / elapsed;
        printf("  BW %llu.%03llu MB/s\n",
               brate / 1000000,
               (brate % 1000000) / 1000);
    } else {
        printf("Wrote %llu B (%d I/Os)\n",
               bcount,
               iocount);
    }

out:

    if ( iobuf ) {
        free(iobuf);
    }

    if ( fd > 0 ) {
        close(fd);
    }

    if ( zones ) {
        free(zones);
    }

    zbc_close(dev);

    return( ret );

}

