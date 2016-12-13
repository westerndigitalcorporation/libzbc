/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public License version 3, "as is," without technical
 * support, and WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
 * received a copy of the GNU Lesser General Public License along with libzbc.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>
#include "zbc_zone_op.h"

static const char *zbc_zone_op_name(enum zbc_zone_op op)
{
	switch (op) {
	case ZBC_OP_RESET_ZONE:
		return "reset";
	case ZBC_OP_OPEN_ZONE:
		return "open";
	case ZBC_OP_CLOSE_ZONE:
		return "close";
	case ZBC_OP_FINISH_ZONE:
		return "finish";
	default:
		return NULL;
	}
}

int zbc_zone_op(char *bin_name, enum zbc_zone_op op,
		int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	zbc_zone_t *zones = NULL, *tgt = NULL;;
	long long start = 0;
	unsigned long long start_sector;
	unsigned int flags = 0;
	int i, ret = 1;
	unsigned int nr_zones, tgt_idx;
	bool sector_unit = false;
	bool lba_unit = false;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev> <zone>\n"
		       "  By default <zone> is interpreted as a zone number.\n"
		       "  If the -lba option is used, <zone> is interpreted\n"
		       "  as the start LBA of the target zone. If the\n"
		       "  -sector option is used, <zone> is interpreted as\n"
		       "  the start 512B sector of the target zone. If the\n"
		       "  -all option is used, <zone> is ignored\n"
		       "Options:\n"
		       "  -v      : Verbose mode\n"
		       "  -sector : Interpret <zone> as a zone start sector\n"
		       "  -lba    : Interpret <zone> as a zone start LBA\n"
		       "  -all    : Operate on all sequential zones\n",
		       bin_name);
		return 1;
	}

	/* Parse options */
	for (i = 0; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-sector") == 0) {

			sector_unit = true;

		} else if (strcmp(argv[i], "-lba") == 0) {

			lba_unit = true;

		} else if (strcmp(argv[i], "-all") == 0) {

			flags |= ZBC_OP_ALL_ZONES;

		} else if ( argv[i][0] == '-' ) {

			printf("Unknown option \"%s\"\n",
			       argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (lba_unit && sector_unit) {
		fprintf(stderr, "-lba and -sector cannot be used together\n");
		return 1;
	}

	if (i != (argc - 2))
		goto usage;

	/* Open device */
	path = argv[i];
	ret = zbc_open(path, O_RDWR, &dev);
	if (ret != 0)
		return 1;

	zbc_get_device_info(dev, &info);

	printf("Device %s: %s\n",
	       path,
	       info.zbd_vendor_id);
	printf("    %s interface, %s disk model\n",
	       zbc_disk_type_str(info.zbd_type),
	       zbc_disk_model_str(info.zbd_model));
	printf("    %llu 512-bytes sectors\n",
	       (unsigned long long) info.zbd_sectors);
	printf("    %llu logical blocks of %u B\n",
	       (unsigned long long)info.zbd_lblocks,
	       (unsigned int)info.zbd_lblock_size);
	printf("    %llu physical blocks of %u B\n",
	       (unsigned long long)info.zbd_pblocks,
	       (unsigned int)info.zbd_pblock_size);
	printf("    %.03F GB capacity\n",
	       (double)(info.zbd_sectors << 9) / 1000000000);

	/* Get target zone */
	start = strtoll(argv[i + 1], NULL, 10);
	if (start < 0) {
		fprintf(stderr, "Invalid zone\n");
		ret = 1;
		goto out;
	}
	if (lba_unit)
		start_sector = zbc_lba2sect(&info, start);
	else if (sector_unit)
		start_sector = start;
	else
		start_sector = 0;

	if (flags & ZBC_OP_ALL_ZONES) {

		printf("Operating on all zones...\n");
		start_sector = 0;

	} else {

		/* Get zone list */
		ret = zbc_list_zones(dev, start, ZBC_RO_ALL, &zones, &nr_zones);
		if ( ret != 0 ) {
			fprintf(stderr, "zbc_list_zones failed\n");
			ret = 1;
			goto out;
		}

		if (lba_unit || sector_unit) {
			/* Search target zone */
			for (i = 0; i < (int)nr_zones; i++) {
				if (start_sector >= zbc_zone_start(&zones[i]) &&
				    start_sector < zbc_zone_start(&zones[i]) + zbc_zone_length(&zones[i])) {
					tgt = &zones[i];
					tgt_idx = i;
					break;
				}
			}
		} else if (start < nr_zones) {
			tgt = &zones[start];
			tgt_idx = start;
		}
		if (!tgt) {
			fprintf(stderr, "Target zone not found\n");
			ret = 1;
			goto out;
		}

		if (lba_unit)
			printf("%s zone %d/%d, LBA %llu...\n",
			       zbc_zone_op_name(op),
			       tgt_idx, nr_zones,
			       (unsigned long long)zbc_sect2lba(&info, zbc_zone_start(tgt)));
		else
			printf("%s zone %d/%d, sector %llu...\n",
			       zbc_zone_op_name(op),
			       tgt_idx, nr_zones,
			       (unsigned long long)zbc_zone_start(tgt));

		start_sector = zbc_zone_start(tgt);

	}

	switch (op) {
	case ZBC_OP_RESET_ZONE:
	case ZBC_OP_OPEN_ZONE:
	case ZBC_OP_CLOSE_ZONE:
	case ZBC_OP_FINISH_ZONE:
		ret = zbc_zone_operation(dev, start_sector, op, flags);
		if (ret != 0) {
			fprintf(stderr, "zbc_%s_zone failed\n",
				zbc_zone_op_name(op));
			ret = 1;
		}
		break;
	default:
		printf("Unknown operation\n");
		ret = 1;
	}

out:
	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;
}

