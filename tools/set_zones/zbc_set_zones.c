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

/***** Main *****/

int main(int argc,
         char **argv)
{
    struct zbc_device_info info;
    struct zbc_device *dev;
    long long conv_num, conv_sz, zone_sz;
    double conv_p;
    int i, ret = -1;
    char *path;

    /* Check command line */
    if ( argc < 5 ) {
usage:
        printf("Usage: %s [options] <dev> <command> <command arguments>\n"
               "Options:\n"
               "    -v     : Verbose mode\n"
               "Commands:\n"
               "    set_sz <conv zone size (MB)> <zone size (MiB)>  : Specify the total size in MiB of all conventional zones\n"
               "                                                      and the size in MiB of zones\n"
               "    set_ps <conv zone size (%%)> <zone size (MiB)>  : Specify the percentage of the capacity to use for\n"
               "                                                      conventional zones and the size in MiB of zones\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 2); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

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

    printf("Device %s: %s\n",
           path,
           info.zbd_vendor_id);
    printf("    %s interface, %s disk model\n",
           zbc_disk_type_str(info.zbd_type),
           zbc_disk_model_str(info.zbd_model));
    printf("    %llu logical blocks of %u B\n",
           (unsigned long long) info.zbd_logical_blocks,
           (unsigned int) info.zbd_logical_block_size);
    printf("    %llu physical blocks of %u B\n",
           (unsigned long long) info.zbd_physical_blocks,
           (unsigned int) info.zbd_physical_block_size);
    printf("    %.03F GiB capacity\n",
           (double) (info.zbd_physical_blocks * info.zbd_physical_block_size) / 1000000000.0);
    printf("\n");

    /* Process command */
    printf("Setting zones:\n");
    i++;

    if ( strcmp(argv[i], "set_sz") == 0 ) {

        /* Set size */
        if ( i != (argc - 3) ) {
            goto usage;
        }

        /* Get arguments */
        conv_sz = (strtoll(argv[i + 1], NULL, 10) * 1024 * 1024) / info.zbd_logical_block_size;;
        if ( conv_sz < 0 ) {
            fprintf(stderr, "Invalid conventional zones size %s\n",
                    argv[i + 1]);
            ret = 1;
            goto out;
        }

        zone_sz = (strtoll(argv[i + 2], NULL, 10) * 1024 * 1024) / info.zbd_logical_block_size;;
        if ( zone_sz <= 0 ) {
            fprintf(stderr, "Invalid zone size %s\n",
                    argv[i + 2]);
            ret = 1;
            goto out;
        }

    } else if ( strcmp(argv[i], "set_ps") == 0 ) {

        /* Set perc + num */
        if ( i != (argc - 3) ) {
            goto usage;
        }

        /* Get arguments */
        conv_p = strtof(argv[i + 1], NULL);
        if ( (conv_p < 0) || (conv_p >= 100.0) ) {
            fprintf(stderr, "Invalid capacity percentage %s for conventional zones\n",
                    argv[i + 1]);
            ret = 1;
            goto out;
        }

        conv_sz = (long long)((double) info.zbd_logical_blocks * (double) conv_p) / 100;

        zone_sz = (strtoll(argv[i + 2], NULL, 10) * 1024ULL * 1024ULL) / info.zbd_logical_block_size;
        if ( zone_sz == 0 ) {
            fprintf(stderr, "Invalid zone size %s\n",
                    argv[i + 2]);
            ret = 1;
            goto out;
        }


    } else {
        
        fprintf(stderr,
                "Unknown command \"%s\"\n",
                argv[i]);
        goto out;

    }

    if ( conv_sz ) {
        conv_num = conv_sz / zone_sz;
        if ( (! conv_num) || (conv_sz % zone_sz) ) {
            conv_num++;
        }
    } else {
        conv_num = 0;
    }
    conv_sz = zone_sz * conv_num;

    printf("    Zone size: %lld MiB (%lld sectors)\n",
           (zone_sz * info.zbd_logical_block_size) / (1024 * 1024),
           zone_sz);

    printf("    Conventional zones: %lld MiB (%lld sectors), %.02F %% of total capacity), %lld zones\n",
           (conv_sz * info.zbd_logical_block_size) / (1024 * 1024),
           conv_sz,
           100.0 * (double)conv_sz / (double)info.zbd_logical_blocks,
           conv_num);

    printf("    Sequential zones: %llu zones\n",
           (info.zbd_logical_blocks - conv_sz) / zone_sz);

    ret = zbc_set_zones(dev, conv_sz, zone_sz);
    if ( ret != 0 ) {
        fprintf(stderr,
                "zbc_set_zones failed\n");
        ret = 1;
    }
    
    /* Retry getting device info */
    ret = zbc_get_device_info(dev, &info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_get_device_info failed\n");
        return( 1 );
    }
    printf("    %llu logical blocks of %u B\n",
           (unsigned long long) info.zbd_logical_blocks,
           (unsigned int) info.zbd_logical_block_size);
    printf("    %llu physical blocks of %u B\n",
           (unsigned long long) info.zbd_physical_blocks,
           (unsigned int) info.zbd_physical_block_size);
    printf("    %.03F GiB capacity\n",
           (double) (info.zbd_physical_blocks * info.zbd_physical_block_size) / 1000000000.0);

out:
    
    zbc_close(dev);
    
    return( ret );
    
}

