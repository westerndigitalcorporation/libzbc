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

/***** Main *****/

int
main(int argc,
     char **argv)
{
    struct zbc_device_info info;
    unsigned long long lba = 0;
    struct zbc_device *dev;
    enum zbc_reporting_options ro = ZBC_RO_ALL;
    int i, ret = 1;
    zbc_zone_t *z, *zones = NULL;
    unsigned int nr_zones, nz = 0, prtl = 0;
    int num = 0;
    char *path;

    /* Check command line */
    if ( argc < 2 ) {
usage:
        printf("Usage: %s [options] <dev>\n"
               "Options:\n"
               "    -v         : Verbose mode\n"
               "    -lba <lba> : Specify zone start LBA (default is 0)\n"
               "    -ro <opt>  : Reporting Option\n"
               "    -p         : Partial bit\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 1); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( strcmp(argv[i], "-lba") == 0 ) {

            if ( i >= (argc - 1) ) {
                goto usage;
            }
            i++;

            lba = strtoll(argv[i], NULL, 10);

        } else if ( strcmp(argv[i], "-ro") == 0 ) {

            if ( i >= (argc - 1) ) {
                goto usage;
            }
            i++;

            ro = atoi(argv[i]);

            if ( ro < 0 ) {
                goto usage;
            }

        } else if ( strcmp(argv[i], "-p") == 0 ) {

            prtl = ZBC_RO_PARTIAL; 

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

    /* Merging ro */
    ro |= prtl;

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

    /* Get the number of zones */
    ret = zbc_report_nr_zones(dev, lba, ro, &nr_zones);
    if ( ret != 0 ) {
	fprintf(stderr, "[TEST][ERROR],zbc_report_nr_zones at lba %llu, ro 0x%02x failed %d\n",
		(unsigned long long) lba,
		(unsigned int) ro,
		ret);
	ret = 1;
	goto out;
    }

    if ( num ) {
	goto out;
    }

    if ( (! nz) || (nz > nr_zones) ) {
	nz = nr_zones;
    }

    /* Allocate zone array */
    zones = (zbc_zone_t *) malloc(sizeof(zbc_zone_t) * nz);
    if ( ! zones ) {
	fprintf(stderr,
                "[TEST][ERROR],No memory\n");
	ret = 1;
	goto out;
    }
    memset(zones, 0, sizeof(zbc_zone_t) * nz);

    /* Get zone information */
    ret = zbc_report_zones(dev, lba, ro, zones, &nz);
    if ( ret != 0 ) {
	fprintf(stderr,
                "[TEST][ERROR],zbc_report_zones failed %d\n", ret);
	ret = 1;
	goto out;
    }

    for(i = 0; i < (int)nz; i++) {
        z = &zones[i];
        if ( zbc_zone_conventional(z) ) {
            printf("[ZONE_INFO],%05d,0x%x,0x%x,%llu,%llu,N/A\n",
                   i,
                   zbc_zone_type(z),
                   zbc_zone_condition(z),
                   zbc_zone_start_lba(z),
                   zbc_zone_length(z));
        } else {
            printf("[ZONE_INFO],%05d,0x%x,0x%x,%llu,%llu,%llu\n",
                   i,
                   zbc_zone_type(z),
                   zbc_zone_condition(z),
                   zbc_zone_start_lba(z),
                   zbc_zone_length(z),
                   zbc_zone_wp_lba(z));
        }
    }

out:

    if ( ret ) {

            zbc_errno_t zbc_err;
            const char *sk_name;
            const char *ascq_name;

            zbc_errno(dev, &zbc_err);
            sk_name = zbc_sk_str(zbc_err.sk);
            ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);

            printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
            printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);

    }

    if ( zones ) {
        free(zones);
    }

    zbc_close(dev);

    return( ret );

}

