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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <libzbc/zbc.h>

static int zbc_info_usage(FILE *out, char *bin_name)
{
	fprintf(out, "Usage: %s [options] <dev>\n"
		"Options:\n"
		"  -h | --help : Display this help message and exit\n"
		"  -v          : Verbose mode\n"
		"  -scsi       : Force the use of SCSI passthrough commands\n"
		"  -ata        : Force the use of ATA passthrough commands\n"
		"  -e          : Print information for an emulated device\n",
		basename(bin_name));
	return 1;

}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	bool do_fake = false;
	int ret, i, oflags = 0;
	char *path;

	/* Check command line */
	if (argc < 2)
		return zbc_info_usage(stderr, argv[0]);

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return zbc_info_usage(stdout, argv[0]);

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-scsi") == 0) {

			oflags = ZBC_O_DRV_SCSI;

		} else if (strcmp(argv[i], "-ata") == 0) {

			oflags = ZBC_O_DRV_ATA;

		} else if (strcmp(argv[i], "-e") == 0) {

			do_fake = true;

		} else if (argv[i][0] == '-') {

			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			return 1;

		} else {

			break;

		}
	}

	if (i != (argc - 1))
		return zbc_info_usage(stderr, argv[0]);

	if (oflags & ZBC_O_DRV_SCSI && oflags & ZBC_O_DRV_ATA) {
		fprintf(stderr,
			"-scsi and -ata options are mutually exclusive\n");
		return 1;
	}

	if (oflags && do_fake) {
		fprintf(stderr,
			"-e option is mutually exclusive with -scsi and -ata options\n");
		return 1;
	}

	/* Open device */
	path = argv[i];
	if (oflags) {
		ret = zbc_open(path, oflags | O_RDONLY, &dev);
		if (ret != 0) {
			if (ret == -ENODEV)
				goto not_zoned;
			fprintf(stderr, "Open %s failed (%s)\n",
				path, strerror(-ret));
			return 1;
		}
		zbc_get_device_info(dev, &info);
	} else {
		ret = zbc_device_is_zoned(path, do_fake, &info);
		if (ret < 0) {
			fprintf(stderr,
				"zbc_device_is_zoned failed %d (%s)\n",
				ret, strerror(-ret));
			return 1;
		}
		if (ret == 0)
			goto not_zoned;

	}

	printf("Device %s:\n", argv[i]);
	zbc_print_device_info(&info, stdout);

	return 0;
not_zoned:
	printf("%s is not a zoned block device\n", path);

	return 0;
}
