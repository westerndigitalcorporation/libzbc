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
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

/***** Get last zone information (start LBA and size) *****/

static int
zbc_get_last_zone(struct zbc_device *dev,
		  zbc_zone_t *z)
{
    unsigned int nr_zones;
    zbc_zone_t *zones;
    int ret;

    /* Get zone list */
    ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_list_zones failed\n");
	return( -1 );
    }

    memcpy(z, &zones[nr_zones - 1], sizeof(zbc_zone_t));

    free(zones);

    return( 0 );

}

/***** Main *****/

int
main(int argc,
     char **argv)
{
    struct zbc_device_info info;
    struct zbc_device *dev;
    zbc_zone_t last_zone;
    int i, ret = 1;
    char *path;

    /* Check command line */
    if ( argc < 2 ) {
usage:
        printf("Usage: %s <dev>\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 1); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( argv[i][0] == '-' ) {

            printf("Unknown option \"%s\"\n",
                   argv[i]);
            goto usage;

        } else {

            break;

        }

    }

    if ( i != (argc - 1) ) {
        goto usage;
    }

    /* Open device */
    path = argv[i];
    ret = zbc_open(path, O_RDONLY, &dev);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],can't open device\n");
        return( 1 );
    }

    ret = zbc_get_device_info(dev, &info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_get_device_info failed\n");
        goto out;
    }

    ret = zbc_get_last_zone(dev, &last_zone);
    if ( ret < 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_get_last_zone failed\n");
        goto out;
    }

    fprintf(stdout,
            "[TEST][INFO][DEVICE_MODEL],%s\n",
	    zbc_disk_model_str(info.zbd_model));

    fprintf(stdout,
            "[TEST][INFO][MAX_NUM_OF_OPEN_SWRZ],%d\n",
	    info.zbd_max_nr_open_seq_req);

    fprintf(stdout,
            "[TEST][INFO][MAX_LBA],%llu\n",
	    (unsigned long long)info.zbd_logical_blocks - 1);

    fprintf(stdout,
            "[TEST][INFO][URSWRZ],%x\n",
	    info.zbd_flags);

    fprintf(stdout,
	    "[TEST][INFO][LAST_ZONE_LBA],%llu\n",
	    (unsigned long long)zbc_zone_start_lba(&last_zone));

    fprintf(stdout,
	    "[TEST][INFO][LAST_ZONE_SIZE],%llu\n",
	    (unsigned long long)zbc_zone_length(&last_zone));


out:

    zbc_close(dev);

    return( ret );

}

