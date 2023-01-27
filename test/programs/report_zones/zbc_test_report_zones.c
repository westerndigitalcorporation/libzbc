// SPDX-License-Identifier: BSD-2-Clause
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * Author: Masato Suzuki (masato.suzuki@wdc.com)
 *         Damien Le Moal (damien.lemoal@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	unsigned long long lba = 0;
	struct zbc_device *dev;
	enum zbc_zone_reporting_options ro = ZBC_RZ_RO_ALL;
	enum zbc_zone_reporting_options partial = 0;
	int i, ret = 1;
	struct zbc_zone *z, *zones = NULL;
	unsigned int nr_zones;
	unsigned int oflags;

	/* Check command line */
	if (argc < 2) {
usage:
		fprintf(stderr,
			"Usage: %s [options] <dev>\n"
			"Options:\n"
			"    -v         : Verbose mode\n"
			"    -lba <lba> : Specify zone start LBA (default is 0)\n"
			"    -ro <opt>  : Reporting Option\n"
			"    -p         : Partial report\n",
			argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-lba") == 0) {

			if (i >= argc - 1)
				goto usage;
			i++;

			lba = strtoll(argv[i], NULL, 10);

		} else if (strcmp(argv[i], "-ro") == 0) {

			if (i >= argc - 1)
				goto usage;
			i++;

			ro = atoi(argv[i]);
			if (ro < 0)
				goto usage;

		} else if (strcmp(argv[i], "-p") == 0) {

			partial = ZBC_RZ_RO_PARTIAL;

		} else if (argv[i][0] == '-') {

			printf("Unknown option \"%s\"\n", argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (i != argc - 1)
		goto usage;

	/* Merging ro */
	ro |= partial;

	/* Open device */
	oflags = ZBC_O_DEVTEST;
	oflags |= ZBC_O_DRV_ATA;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(argv[i], oflags | O_RDONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed, err %d (%s) %s\n",
			ret, strerror(-ret), argv[i]);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	/* Get the number of zones */
	ret = zbc_report_nr_zones(dev, zbc_lba2sect(&info, lba), ro, &nr_zones);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],zbc_report_nr_zones lba %llu, ro 0x%02x failed %d\n",
			(unsigned long long)lba,
			(unsigned int)ro,
			ret);
		ret = 1;
		goto out;
	}

	/* Allocate zone array */
	zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));
	if (!zones) {
		fprintf(stderr,
			"[TEST][ERROR],No memory\n");
		ret = 1;
		goto out;
	}

	/* Get zone information */
	ret = zbc_report_zones(dev, lba, ro, zones, &nr_zones);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_report_zones failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	for (i = 0; i < (int)nr_zones; i++) {
		z = &zones[i];
		if (zbc_zone_conventional(z))
			printf("[ZONE_INFO],%d,0x%x,0x%x,%llu,%llu,N/A\n",
			       i,
			       zbc_zone_type(z),
			       zbc_zone_condition(z),
			       zbc_sect2lba(&info, zbc_zone_start(z)),
			       zbc_sect2lba(&info, zbc_zone_length(z)));
		else
			printf("[ZONE_INFO],%d,0x%x,0x%x,%llu,%llu,%llu\n",
			       i,
			       zbc_zone_type(z),
			       zbc_zone_condition(z),
			       zbc_sect2lba(&info, zbc_zone_start(z)),
			       zbc_sect2lba(&info, zbc_zone_length(z)),
			       zbc_sect2lba(&info, zbc_zone_wp(z)));
	}

out:
	if (ret != 0) {
		struct zbc_errno zbc_err;
		const char *sk_name;
		const char *ascq_name;

		zbc_errno(dev, &zbc_err);
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);

		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);

	}

	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;
}

