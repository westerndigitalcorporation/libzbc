// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

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

static int zbc_zone_op_usage(FILE *out, char *bin_name)
{
	fprintf(out,
		"Usage: %s [options] <dev> [<zone>] [<zone count>]\n"
		"  By default <zone> is interpreted as a zone number.\n"
		"  If the -lba option is used, <zone> is interpreted\n"
		"  as the start LBA of the target zone. If the\n"
		"  -sector option is used, <zone> is interpreted as\n"
		"  the start 512B sector of the target zone. If the\n"
		"  -all option is used, <zone> is ignored\n"
		"  If <zone count> is omitted, the operation is taken place\n"
		"  at the <zone>. Otherwise, it defines the number of\n"
		"  consecutive zones starting from the <zone> on which the\n"
		"  zone operation is performed\n"
		"Options:\n"
		"  -h | --help : Display this help message and exit\n"
		"  -v          : Verbose mode\n"
		"  -scsi       : Force the use of SCSI passthrough commands\n"
		"  -ata        : Force the use of ATA passthrough commands\n"
		"  -sector     : Interpret <zone> as a zone start sector\n"
		"  -lba        : Interpret <zone> as a zone start LBA\n"
		"  -all        : Operate on all sequential zones\n",
		basename(bin_name));

	return 1;
}

int zbc_zone_op(char *bin_name, enum zbc_zone_op op,
		int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	struct zbc_zone *zones = NULL, *tgt = NULL;
	long long start = -1LL;
	unsigned long long start_sector = -1ULL;
	unsigned int flags = 0;
	int i, ret = 1, oflags = 0, zone_count = 0;
	unsigned int nr_zones, tgt_idx;
	bool sector_unit = false;
	bool lba_unit = false;
	char *path, *end;

	/* Check command line */
	if (!argc)
		return zbc_zone_op_usage(stderr, bin_name);

	/* Parse options */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return zbc_zone_op_usage(stdout, bin_name);

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-scsi") == 0) {

			oflags = ZBC_O_DRV_SCSI;

		} else if (strcmp(argv[i], "-ata") == 0) {

			oflags = ZBC_O_DRV_ATA;

		} else if (strcmp(argv[i], "-sector") == 0) {

			sector_unit = true;

		} else if (strcmp(argv[i], "-lba") == 0) {

			lba_unit = true;

		} else if (strcmp(argv[i], "-all") == 0) {

			flags |= ZBC_OP_ALL_ZONES;

		} else if ( argv[i][0] == '-' ) {

			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			return 1;

		} else {

			break;

		}
	}

	if (i > argc - 1) {
		fprintf(stderr, "No device specified\n");
		return 1;
	}
	path = argv[i];

	if (oflags & ZBC_O_DRV_SCSI && oflags & ZBC_O_DRV_ATA) {
		fprintf(stderr,
			"-scsi and -ata options are mutually exclusive\n");
		return 1;
	}

	if (lba_unit && sector_unit) {
		fprintf(stderr, "-lba and -sector cannot be used together\n");
		return 1;
	}

	if (flags & ZBC_OP_ALL_ZONES) {
		if (i != argc - 1) {
			fprintf(stderr, "Too many arguments\n");
			return 1;
		}
	} else {
		if (i >= argc - 1) {
			fprintf(stderr, "No zone specified\n");
			return 1;
		}
		if (i < argc - 3) {
			fprintf(stderr, "Too many arguments\n");
			return 1;
		}

		/* Get target zone */
		start = strtoll(argv[i + 1], &end, 10);
		if (*end != '\0' || start < 0) {
			fprintf(stderr, "Invalid zone\n");
			return 1;
		}
		i++;

		if (i < argc - 1) {
			zone_count = strtol(argv[i + 1], &end, 10);
			if (*end != '\0' || zone_count < 0) {
				fprintf(stderr, "Invalid zone count\n");
				return 1;
			}
		}
	}

	/* Open device */
	ret = zbc_open(path, oflags | O_RDWR, &dev);
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

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	if (flags & ZBC_OP_ALL_ZONES) {

		printf("Operating on all zones...\n");

	} else {

		if (lba_unit)
			start_sector = zbc_lba2sect(&info, start);
		else if (sector_unit)
			start_sector = start;

		/* Get zone list */
		ret = zbc_list_zones(dev, 0, ZBC_RZ_RO_ALL, &zones, &nr_zones);
		if ( ret != 0 ) {
			fprintf(stderr, "zbc_list_zones failed\n");
			ret = 1;
			goto out;
		}

		if (lba_unit || sector_unit) {
			/* Search target zone */
			for (tgt_idx = 0; tgt_idx < nr_zones; tgt_idx++) {
				tgt = &zones[tgt_idx];
				if (start_sector >= zbc_zone_start(tgt) &&
				    start_sector < zbc_zone_start(tgt) +
						    zbc_zone_length(tgt)) {
					break;
				}
			}
			if (tgt_idx == nr_zones)
				tgt = NULL;
		} else if (start < nr_zones) {
			tgt = &zones[start];
			tgt_idx = start;
			start_sector = zbc_zone_start(tgt);
		}
		if (!tgt) {
			fprintf(stderr, "Target zone not found\n");
			ret = 1;
			goto out;
		}

		if (lba_unit)
			printf("%s zone %d/%d, LBA %llu, %d zone(s)...\n",
			       zbc_zone_op_name(op),
			       tgt_idx, nr_zones,
			       (unsigned long long)zbc_sect2lba(&info, zbc_zone_start(tgt)),
			       zone_count > 1 ? zone_count : 1);
		else
			printf("%s zone %d/%d, sector %llu, %d zone(s)...\n",
			       zbc_zone_op_name(op),
			       tgt_idx, nr_zones,
			       (unsigned long long)zbc_zone_start(tgt),
			       zone_count > 1 ? zone_count : 1);

	}

	switch (op) {
	case ZBC_OP_RESET_ZONE:
	case ZBC_OP_OPEN_ZONE:
	case ZBC_OP_CLOSE_ZONE:
	case ZBC_OP_FINISH_ZONE:
		ret = zbc_zone_group_op(dev, start_sector, zone_count,
					op, flags);
		if (ret != 0) {
			fprintf(stderr, "zbc_%s_zone failed\n",
				zbc_zone_op_name(op));
			ret = 1;
		}
		break;
	default:
		fprintf(stderr, "Unknown operation\n");
		ret = 1;
	}

out:
	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;
}

