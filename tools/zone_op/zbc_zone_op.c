// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital COrporation or its affiliates.
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
	struct zbc_zone *zones = NULL, *tgt = NULL;;
	long long start = -1LL;
	unsigned long long start_sector = -1ULL;
	unsigned int flags = 0;
	int i, ret = 1;
	unsigned int nr_zones, tgt_idx;
	bool sector_unit = false;
	bool lba_unit = false;
	char *path, *end;

	/* Check command line */
	if (!argc) {
usage:
		printf("Usage: %s [options] <dev> [<zone>]\n"
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

	if (i > argc - 1) {
		fprintf(stderr, "No device specified\n");
		return 1;
	}

	if (lba_unit && sector_unit) {
		fprintf(stderr, "-lba and -sector cannot be used together\n");
		return 1;
	}

	/* Open device */
	path = argv[i];
	ret = zbc_open(path, O_RDWR, &dev);
	if (ret != 0) {
		if (ret == -ENODEV)
			fprintf(stderr,
				"Open %s failed (not a zoned block device)\n",
				path);
		else
			fprintf(stderr, "Open %s failed (%s)\n",
				path, strerror(-ret));
		return 1;
	}

	if (flags & ZBC_OP_ALL_ZONES) {
		if (i != argc - 1) {
			fprintf(stderr, "Too many arguments\n");
			return 1;
		}
	} else {
		if (argc < 2 || i < argc - 2) {
			fprintf(stderr, "No zone specified\n");
			return 1;
		}
		if (i > argc - 2) {
			fprintf(stderr, "Too many arguments\n");
			return 1;
		}
	}

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	if (flags & ZBC_OP_ALL_ZONES) {

		printf("Operating on all zones...\n");
		start_sector = 0;

	} else {

		/* Get target zone */
		start = strtoll(argv[i + 1], &end, 10);
		if (*end != '\0' || start < 0) {
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

