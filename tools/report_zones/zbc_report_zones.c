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

int main(int argc,
         char **argv)
{
    struct zbc_device_info info;
    unsigned long long lba = 0, nr_lba = 0;
    struct zbc_device *dev;
    enum zbc_reporting_options ro = ZBC_RO_ALL;
    int i, ret = 1;
    zbc_zone_t *z, *zones = NULL;
    unsigned int nr_zones = 0, nz = 0, partial = 0;
    int num = 0;
    char *path;

    /* Check command line */
    if ( argc < 2 ) {
usage:
        printf("Usage: %s [options] <dev>\n"
               "Options:\n"
               "    -v         : Verbose mode\n"
               "    -n         : Get only the number of zones\n"
               "    -nz <num>  : Get at most <num> zones\n"
               "    -lba <lba> : Specify zone start LBA (default is 0)\n"
               "    -ro <opt>  : Specify reporting option: \"all\", \"empty\",\n"
               "                 \"imp_open\", \"exp_open\", \"closed\", \"full\",\n"
               "                 \"rdonly\", \"offline\", \"reset\", \"non_seq\" or \"not_wp\".\n"
               "                 Default is \"all\"\n"
               "    -p         : Partial bit\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 1); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( strcmp(argv[i], "-n") == 0 ) {

            num = 1;

	} else if ( strcmp(argv[i], "-nz") == 0 ) {

            if ( i >= (argc - 1) ) {
                goto usage;
            }
            i++;

            nz = strtol(argv[i], NULL, 10);
	    if ( nz <= 0 ) {
		goto usage;
	    }

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

            if ( strcmp(argv[i], "all") == 0 ) {
                ro = ZBC_RO_ALL;
            } else if ( strcmp(argv[i], "empty") == 0 ) {
                ro = ZBC_RO_EMPTY;
            } else if ( strcmp(argv[i], "imp_open") == 0 ) {
                ro = ZBC_RO_IMP_OPEN;
            } else if ( strcmp(argv[i], "exp_open") == 0 ) {
                ro = ZBC_RO_EXP_OPEN;
            } else if ( strcmp(argv[i], "closed") == 0 ) {
                ro = ZBC_RO_CLOSED;
            } else if ( strcmp(argv[i], "full") == 0 ) {
                ro = ZBC_RO_FULL;
            } else if ( strcmp(argv[i], "rdonly") == 0 ) {
                ro = ZBC_RO_RDONLY;
            } else if ( strcmp(argv[i], "offline") == 0 ) {
                ro = ZBC_RO_OFFLINE;
            } else if ( strcmp(argv[i], "reset") == 0 ) {
                ro = ZBC_RO_RESET;
            } else if ( strcmp(argv[i], "non_seq") == 0 ) {
                ro = ZBC_RO_NON_SEQ;
            } else if ( strcmp(argv[i], "not_wp") == 0 ) {
                ro = ZBC_RO_NOT_WP;
            } else {
                fprintf(stderr, "Unknown zone reporting option \"%s\"\n",
                        argv[i]);
                goto usage;
            }

        } else if ( strcmp(argv[i], "-p") == 0 ) {

            partial = ZBC_RO_PARTIAL;

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
    printf("    %.03F GB capacity\n",
           (double) (info.zbd_physical_blocks * info.zbd_physical_block_size) / 1000000000);

    /* Get the number of zones */
    ro |= partial;
    ret = zbc_report_nr_zones(dev, lba, ro, &nr_zones);
    if ( ret != 0 ) {
	fprintf(stderr, "zbc_report_nr_zones at lba %llu, ro 0x%02x failed %d\n",
		(unsigned long long) lba,
		(unsigned int) ro,
		ret);
	ret = 1;
	goto out;
    }

    /* Print zone info */
    printf("    %u zone%s from LBA %llu, reporting option 0x%02x\n",
	   nr_zones,
	   (nr_zones > 1) ? "s" : "",
	   lba,
	   ro);
    if ( info.zbd_model == ZBC_DM_HOST_MANAGED ) {
	printf("    Maximum number of open sequential write required zones: %u\n",
	       (unsigned int) info.zbd_max_nr_open_seq_req);
    } else {
	printf("    Optimal number of open sequential write preferred zones: %u\n",
	       (unsigned int) info.zbd_opt_nr_open_seq_pref);
	printf("    Optimal number of non-sequentially written sequential write preferred zones: %u\n",
	       (unsigned int) info.zbd_opt_nr_non_seq_write_seq_pref);
    }

    if ( num ) {
	goto out;
    }

    if ( (! nz) || (nz > nr_zones) ) {
	nz = nr_zones;
    }

    if ( ! nz ) {
	goto out;
    }

    /* Allocate zone array */
    zones = (zbc_zone_t *) malloc(sizeof(zbc_zone_t) * nz);
    if ( ! zones ) {
	fprintf(stderr, "No memory\n");
	ret = 1;
	goto out;
    }
    memset(zones, 0, sizeof(zbc_zone_t) * nz);

    /* Get zone information */
    ret = zbc_report_zones(dev, lba, ro, zones, &nz);
    if ( ret != 0 ) {
	fprintf(stderr, "zbc_list_zones failed %d\n", ret);
	ret = 1;
	goto out;
    }

    printf("%u / %u zone%s:\n", nz, nr_zones, (nz > 1) ? "s" : "");
    lba = 0;
    for(i = 0; i < (int)nz; i++) {

        z = &zones[i];

	if (ro == ZBC_RO_ALL) {
	    /* Check */
	    if (zbc_zone_start_lba(z) != lba) {
		printf("[WARNING] Zone %05d: LBA %llu should be %llu\n",
		       i,
		       zbc_zone_start_lba(z),
		       lba);
		lba = zbc_zone_start_lba(z);
	    }
	    nr_lba += zbc_zone_length(z);
	    lba += zbc_zone_length(z);
	}

        if ( zbc_zone_conventional(z) ) {
            printf("Zone %05d: type 0x%x (%s), cond 0x%x (%s), LBA %llu, "
		   "%llu sectors, wp N/A\n",
                   i,
                   zbc_zone_type(z),
                   zbc_zone_type_str(zbc_zone_type(z)),
                   zbc_zone_condition(z),
                   zbc_zone_condition_str(zbc_zone_condition(z)),
                   zbc_zone_start_lba(z),
                   zbc_zone_length(z));
	    continue;
        }

        if ( zbc_zone_sequential(z) ) {
	    printf("Zone %05d: type 0x%x (%s), cond 0x%x (%s), need_reset %d, "
		   "non_seq %d, LBA %llu, %llu sectors, wp %llu\n",
		   i,
		   zbc_zone_type(z),
		   zbc_zone_type_str(zbc_zone_type(z)),
		   zbc_zone_condition(z),
		   zbc_zone_condition_str(zbc_zone_condition(z)),
		   zbc_zone_need_reset(z),
		   zbc_zone_non_seq(z),
		   zbc_zone_start_lba(z),
		   zbc_zone_length(z),
		   zbc_zone_wp_lba(z));
	    continue;
        }

	printf("Zone %05d: unknown type 0x%x, LBA %llu, %llu sectors\n",
	       i,
	       zbc_zone_type(z),
	       zbc_zone_start_lba(z),
	       zbc_zone_length(z));

    }

    if ( ro == ZBC_RO_ALL ) {
	/* Check */
	if ( nr_lba != info.zbd_logical_blocks ) {
	    printf("[WARNING] %llu logical blocks reported "
		   "but capacity is %llu logical blocks\n",
		   nr_lba,
		   (unsigned long long)info.zbd_logical_blocks);
	}
    }

out:

    if ( zones ) {
        free(zones);
    }

    zbc_close(dev);

    return( ret );

}

