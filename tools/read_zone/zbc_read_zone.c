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
static int zbc_read_zone_abort = 0;

/**
 * System time in usecs.
 */
static __inline__ unsigned long long
zbc_read_zone_usec(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return( (unsigned long long) tv.tv_sec * 1000000LL + (unsigned long long) tv.tv_usec );

}

/**
 * Signal handler.
 */
static void
zbc_read_zone_sigcatcher(int sig)
{

    zbc_read_zone_abort = 1;

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
    unsigned long long brate;
    int zidx;
    int fd = -1, i, ret = 1;
    size_t iosize;
    void *iobuf = NULL;
    uint64_t lba;
    uint32_t lba_count;
    int ionum = 0;
    struct zbc_zone *zones = NULL;
    struct zbc_zone *iozone = NULL;
    unsigned int nr_zones, iocount = 0;
    char *path, *file = NULL;
    long long lba_ofst = -1; //Offset in nb of lba from the starting lba of th zone to read
    long long lba_max = 0;

    /* Check command line */
    if ( argc < 4 ) {
usage:
        printf("Usage: %s [options] <dev> <zone no> <I/O size>\n"
               "  Read a zone up to the current write pointer\n"
               "  or the number of I/O specified is executed\n"
               "Options:\n"
               "    -v         : Verbose mode\n"
               "    -nio <num> : Limit the number of I/O executed to <num>\n"
               "    -f <file>  : Write the content of the zone to <file>\n"
               "                 If <file> is \"-\", the zone content is\n"
               "                 written to the standard output\n"
               "     -lba      : lba offset from the starting lba of the zone <zone no>.\n",
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
    signal(SIGQUIT, zbc_read_zone_sigcatcher);
    signal(SIGINT, zbc_read_zone_sigcatcher);
    signal(SIGTERM, zbc_read_zone_sigcatcher);

    /* Open device */
    ret = zbc_open(path, O_RDONLY, &dev);
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

        if ( strcmp(file, "-") == 0 ) {

            freopen(NULL, "wb", stdout);
            fd = fileno(stdout);
            printf("Writting target zone %d to standard output, %zu B I/Os\n",
                   zidx,
                   iosize);

        } else {

            fd = open(file, O_CREAT | O_TRUNC | O_LARGEFILE | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP);
            if ( fd < 0 ) {
                fprintf(stderr, "Open file \"%s\" failed %d (%s)\n",
                        file,
                        errno,
                        strerror(errno));
                ret = 1;
                goto out;
            }

            printf("Writting target zone %d to file \"%s\", %zu B I/Os\n",
                   zidx,
                   file,
                   iosize);

        }

    } else if ( ! ionum ) {

        printf("Reading target zone %d, %zu B I/Os\n",
               zidx,
               iosize);

    } else {

        printf("Reading target zone %d, %d I/Os of %zu B\n",
               zidx,
               ionum,
               iosize);

    }

    if ( lba_ofst >= 0 ) {
        /* lba offset was given as input.
         * Then there is no limitation on the lba unless a number of IO (option -nio)
         * was given as input. In that case lba_max = iozone + lba_ofst + ionum * io size.
         */
        lba_max = info.zbd_physical_blocks;
    } else {
        /* No lba offset given as input.
         * The limitation is given by the lba of the write pointer.
         * Or if a number of IO was given as input it will be min(wp_lba, ionum).
         */
        lba_max = zbc_zone_wp_lba(iozone);
        lba_ofst = 0;
    }

    lba = iozone->zbz_start;//Used to control current lba versus lba_max

    elapsed = zbc_read_zone_usec();

    while( (! zbc_read_zone_abort)
           && (lba < lba_max) ) {

        /* Read zone */
        lba_count = iosize / info.zbd_physical_block_size;

        if (  (lba + lba_count) >= lba_max ) {
            lba_count = lba_max - lba;
        }

        ret = zbc_pread(dev, iozone, iobuf, lba_count, lba_ofst);
        if ( ret <= 0 ) {
            ret = 1;
            break;
        }
        lba_count = ret;

        if ( file ) {
            /* Write file */
            ret = write(fd, iobuf, lba_count * info.zbd_physical_block_size);
            if ( ret < 0 ) {
                fprintf(stderr, "Write file \"%s\" failed %d (%s)\n",
                        file,
                        errno,
                        strerror(errno));
                ret = 1;
                break;
            }
            ret = 0;
        }

        /* Update the value of the current lba and the value of the next offset. */
        lba      += lba_count;
        lba_ofst += lba_count;//Shift the offset by the number of read lba

        bcount += lba_count * info.zbd_physical_block_size;
        iocount++;
        ret = 0;

        /* If a number of IO was given as input then
         * lba_max = iozone + lba_ofst + ionum
         */
        if ( (ionum > 0) && (iocount >= ionum) ) {
            break;
        }

    }

    elapsed = zbc_read_zone_usec() - elapsed;

    if ( elapsed ) {
        printf("Read %llu B (%d I/Os) in %llu.%03llu sec\n",
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
        printf("Read %llu B (%d I/Os)\n",
               bcount,
               iocount);
    }

out:

    if ( file && (fd > 0) ) {
        if ( fd != fileno(stdout) ) {
            close(fd);
        }
        if ( ret != 0 ) {
            unlink(file);
        }
    }

    if ( iobuf ) {
        free(iobuf);
    }

    if ( zones ) {
        free(zones);
    }

    zbc_close(dev);

    return( ret );

}

