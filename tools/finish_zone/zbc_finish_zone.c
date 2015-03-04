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

/***** Main *****/

int main(int argc,
         char **argv)
{
    struct zbc_device_info info;
    long long z;
    struct zbc_device *dev;
    int i, lba = 0, ret = 1;
    zbc_zone_t *zones = NULL, *rzone = NULL;;
    unsigned int nr_zones, rzone_idx = -1;
    char *path;

    /* Check command line */
    if ( argc < 2 ) {
usage:
        printf("Usage: %s [options] <dev> <zone>\n"
	       "    By default <zone> is interpreted as a zone number.\n"
	       "    If the -lba option is used, <zone> is interpreted as\n"
	       "    the start LBA of the zone to finish.\n"
	       "    If <zone> is -1, all zones are finished.\n"
               "Options:\n"
               "    -v   : Verbose mode\n"
               "    -lba : Interpret <zone> as a zone start LBA instead of a zone nmumber\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 1); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( strcmp(argv[i], "-lba") == 0 ) {
            
            lba = 1;

        } else if ( argv[i][0] == '-' ) {
            
            printf("Unknown option \"%s\"\n",
                   argv[i]);
            goto usage;

        } else {

            break;

        }

    }

    if ( i != (argc - 2) ) {
        goto usage;
    }

    /* Open device */
    path = argv[i];
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
           (double) (info.zbd_physical_blocks * info.zbd_physical_block_size) / 1000000000);

    /* Target zone */
    z = strtoll(argv[i + 1], NULL, 10);
    if ( z == -1 ) {

        printf("Finishing all zones...\n");

    } else {

        /* Get zone list */
        ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
        if ( ret != 0 ) {
            fprintf(stderr, "zbc_list_zones failed\n");
            ret = 1;
            goto out;
        }

        /* Search target zone */
        if ( lba == 0 ) {
            if ( (z >= 0) && (z < nr_zones) ) {
                rzone = &zones[z];
                rzone_idx = z;
            }
        } else {
            for(i = 0; i < (int)nr_zones; i++) {
                if ( zones[i].zbz_start == (uint64_t)z ) {
                    rzone = &zones[i];
                    rzone_idx = i;
                    break;
                }
            }
        }
        if ( ! rzone ) {
            fprintf(stderr, "Target zone not found\n");
            ret = 1;
            goto out;
        }

        printf("Finishing zone %d/%d, start LBA %llu...\n",
               rzone_idx,
               nr_zones,
               (unsigned long long) rzone->zbz_start);

        z = rzone->zbz_start;

    }
        
    /* Finish zone */
    ret = zbc_finish_zone(dev, z);
    if ( ret != 0 ) {
        fprintf(stderr,
                "zbc_finish_zone failed\n");
        ret = 1;
    }

out:

    if ( zones ) {
        free(zones);
    }

    /* Close device file */
    zbc_close(dev);

    return( ret );

}

