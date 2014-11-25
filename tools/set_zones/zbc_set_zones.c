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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

#include <zbc_private.h>

/***** Private function *****/

static int
zbc_set_zones_align(unsigned long long sectors,
                    unsigned long long sector_size,
                    unsigned long long align,
                    unsigned long long *cz_size,
                    unsigned long long *sz_size);

static void
zbc_display_zones_sizes(unsigned long long sectors,
                        unsigned long long sector_size,
                        unsigned long long cz_size,
                        unsigned long long sz_size);

/***** Main *****/

int main(int argc,
         char **argv)
{
    unsigned long long conv_sz, seq_sz = 0;
    long long align = 0;
    double conv_p;
    int seq_num;
    struct zbc_device_info info;
    struct zbc_device *dev;
    int i, ret = -1;
    char *path;

    /* Check command line */
    if ( argc < 5 ) {
usage:
        printf("Usage: %s [options] <dev> <command> <command arguments>\n"
               "Options:\n"
               "    -v     : Verbose mode\n"
               "    -a <x> : Force alignment of zone sizes on a multiple of x kB\n"
               "Commands:\n"
               "    set_sz <conv zone size> <seq zone size>             : Specify the size of the conventional zone and sequential\n"
               "                                                          write required zones in number of logical sectors\n"
               "    set_pn <conv zone percentage> <number of seq zones> : Specify the percentage of the disk capacity to allocate\n"
               "                                                          to the conventional zone and the number of sequential\n"
               "                                                          write required zones\n"
               "    set_ps <conv zone percentage> <seq zone size (MB)>  : Specify the percentage of the disk capacity to allocate\n"
               "                                                          to the conventional zone and the size in MiB of sequential\n"
               "                                                          write required zones\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 2); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( strcmp(argv[i], "-a") == 0 ) {
            
            i++;
            if ( i >= (argc - 2) ) {
                goto usage;
            }
            
            align = (atoi(argv[i])) * 1024;
            if ( align <= 0 ) {
                fprintf(stderr,
                        "Invalid align value %s\n",
                        argv[i]);
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

    if ( i > (argc - 2) ) {
        goto usage;
    }

    /* Open device */
    path = argv[i];
    ret = zbc_open(path, O_RDONLY, &dev);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_open failed\n");
        return( 1 );
    }

    /* Get device info */
    ret = zbc_get_device_info(dev, &info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_get_device_info failed\n");
        return( 1 );
    }

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

    /* Process command */
    printf("Setting zones:\n");
    i++;

    if ( strcmp(argv[i], "set_sz") == 0 ) {

        /* Set size */
        if ( i != (argc - 3) ) {
            goto usage;
        }

        /* Get arguments */
        conv_sz = strtoll(argv[i + 1], NULL, 10);
        seq_sz = strtoll(argv[i + 2], NULL, 10);
        if ( seq_sz == 0 ) {
            fprintf(stderr, "Invalid size %s for sequential write required zones\n",
                    argv[i + 2]);
            ret = 1;
            goto out;
        }

    } else if ( strcmp(argv[i], "set_pn") == 0 ) {

        /* Set perc + num */
        if ( i != (argc - 3) ) {
            goto usage;
        }

        /* Get arguments */
        conv_p = strtof(argv[i + 1], NULL);
        if ( conv_p >= 100.0 ) {
            fprintf(stderr, "Invalid capacity percentage %s for conventional zone\n",
                    argv[i + 1]);
            ret = 1;
            goto out;
        }

        seq_num = atoi(argv[i + 2]);
        if ( seq_num <= 0 ) {
            fprintf(stderr, "Invalid number of sequential write required zones %s\n",
                    argv[i + 2]);
            ret = 1;
            goto out;
        }

        conv_sz = (unsigned long long)((double) info.zbd_logical_blocks * (double) conv_p) / 100;
        seq_sz = (info.zbd_logical_blocks - conv_sz) / seq_num;

    } else if ( strcmp(argv[i], "set_ps") == 0 ) {

        /* Set perc + num */
        if ( i != (argc - 3) ) {
            goto usage;
        }

        /* Get arguments */
        conv_p = strtof(argv[i + 1], NULL);
        if ( conv_p >= 100.0 ) {
            fprintf(stderr, "Invalid capacity percentage %s for conventional zone\n",
                    argv[i + 1]);
            ret = 1;
            goto out;
        }

        seq_sz = (strtoll(argv[i + 2], NULL, 10) * 1024ULL * 1024ULL) / info.zbd_logical_block_size;
        if ( seq_sz == 0 ) {
            fprintf(stderr, "Invalid size %s for sequential write required zones\n",
                    argv[i + 2]);
            ret = 1;
            goto out;
        }

        conv_sz = (unsigned long long)((double) info.zbd_logical_blocks * (double) conv_p) / 100;
        seq_num = (info.zbd_logical_blocks - conv_sz) / seq_sz;

    } else {
        
        fprintf(stderr,
                "Unknown command \"%s\"\n",
                argv[i]);
        goto out;

    }

    if ( align ) {
        ret = zbc_set_zones_align(info.zbd_logical_blocks,
                                  info.zbd_logical_block_size,
                                  align,
                                  &conv_sz,
                                  &seq_sz);
        if ( ret ) {
            goto out;
        }
    }

    zbc_display_zones_sizes(info.zbd_physical_blocks, info.zbd_physical_block_size, conv_sz, seq_sz);

    ret = zbc_set_zones(dev, conv_sz, seq_sz);
    if ( ret != 0 ) {
        fprintf(stderr,
                "zbc_set_zones failed\n");
        ret = 1;
    }
    
out:
    
    zbc_close(dev);
    
    return( ret );
    
}

/***** Private function *****/

static int
zbc_set_zones_align(unsigned long long sectors,
                    unsigned long long sector_size,
                    unsigned long long align,
                    unsigned long long *cz_size,
                    unsigned long long *sz_size)
{
    unsigned long long r = 0;
    unsigned long long rem_phy_blocks = 0;
    unsigned long long abs = 0;

    /* Align the zone sizes */
    if ( sector_size % align ) {

        r = ((unsigned long long)((double)(*cz_size) * (double)sector_size)) % align;
        abs = (align < sector_size) ? (r - align) : (align - r);

        if ( r ) {
            *cz_size +=  abs / sector_size;
        }
        rem_phy_blocks = sectors - *cz_size;

        if ( rem_phy_blocks < *sz_size ) {
            printf("    Request alignment cannot be fullfil (1)\n");
            return( 1 );
        }

        if ( *sz_size > 0) {

            r = ((unsigned long long)((double)(*sz_size) * (double)sector_size)) % align;
            abs = (align < sector_size) ? (r - align) : (align - r);

            if ( r ) {
                *sz_size += abs / sector_size;
            }

            if ( rem_phy_blocks < *sz_size ) {
                printf("    Request alignment cannot be fullfil (2)\n");
                return( 1 );
            }

        }

        zbc_display_zones_sizes(sectors, sector_size, *cz_size, *sz_size);

    }

    return( 0 );

}

static void
zbc_display_zones_sizes(unsigned long long sectors,
                        unsigned long long sector_size,
                        unsigned long long cz_size,
                        unsigned long long sz_size)
{
    unsigned long long seq_zones = (sectors - cz_size) / sz_size;

    if ( ((sectors - cz_size) - (seq_zones * sz_size)) > (sz_size / 2) ) {
        seq_zones++;
    }

    printf("    Conventional zone size : %llu sectors (%.02f %% of total capacity)\n"
           "    Sequential zone size   : %llu sectors => %llu zones\n",
           cz_size,
           100 * ((double)cz_size / (double)sectors),
           sz_size,
           seq_zones);

    return;

}
