// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital COrporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 *         Christophe Louargant (christophe.louargant@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <libzbc/zbc.h>

#include <zbc_private.h>

static int zbc_set_write_ptr_usage(char *prog)
{
	printf("Usage: %s [options] <dev> <zone no> <sector (-1 for full)>\n"
	       "Options:\n"
	       "  -h | --help : Display this help message and exit\n"
	       "  -v          : Verbose mode\n",
	       basename(prog));

	return 1;
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	struct zbc_zone *zones = NULL;
	unsigned int nr_zones;
	long long sector;
	int i, z, ret = 1;
	char *path;

	/* Check command line */
	if (argc < 4)
		return zbc_set_write_ptr_usage(argv[0]);

	/* Parse options */
	for (i = 1; i < argc; i++) {

		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return zbc_set_write_ptr_usage(argv[0]);

		if ( strcmp(argv[i], "-v") == 0 ) {

			zbc_set_log_level("debug");

		} else if ( argv[i][0] == '-' ) {

			printf("Unknown option \"%s\"\n", argv[i]);
			return 1;

		} else {

			break;

		}

	}

	if (i != (argc - 3)) {
		fprintf(stderr, "Invalid command line\n");
		return 1;
	}

	/* Open device */
	path = argv[i];
	ret = zbc_open(path, O_RDWR | ZBC_O_DRV_FAKE, &dev);
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
	if (info.zbd_type != ZBC_DT_FAKE) {
		fprintf(stderr,
			"Device %s is not using the fake backend driver\n",
			path);
		ret = 1;
		goto out;
	}

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	/* Get zone list */
	ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
	if (ret != 0) {
		fprintf(stderr, "zbc_list_zones failed\n");
		ret = 1;
		goto out;
	}

	/* Check target zone */
	z = atoi(argv[i + 1]);
	if (z < 0 || z > (int)nr_zones ) {
		fprintf(stderr, "Invalid target zone number\n");
		ret = 1;
		goto out;
	}

	/* Get write pointer sector */
	sector = strtoll(argv[i + 2], NULL, 10);
	if (sector == -1)
		sector = zones[z].zbz_start + zones[z].zbz_length;

	printf("Setting zone %d/%d write pointer sector to %llu...\n",
	       z,
	       nr_zones,
	       (unsigned long long) sector);

	/* Set WP */
	ret = zbc_set_write_pointer(dev, zones[z].zbz_start, sector);
	if (ret != 0) {
		fprintf(stderr,
			"zbc_set_write_pointer failed\n");
		ret = 1;
	}

out:
	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;
}

