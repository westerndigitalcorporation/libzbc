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

static int zbc_info_usage(char *bin_name)
{
	printf("Usage: %s [options] <dev>\n"
	       "Options:\n"
	       "  -h | --help : Display this help message and exit\n"
	       "  -v          : Verbose mode\n"
	       "  -e          : Print information for an emulated device\n",
	       basename(bin_name));
	return 1;

}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	bool do_fake = false;
	int ret, i;

	/* Check command line */
	if (argc < 2)
		return zbc_info_usage(argv[0]);

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return zbc_info_usage(argv[0]);

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-e") == 0) {

			do_fake = true;

		} else if (argv[i][0] == '-') {

			printf("Unknown option \"%s\"\n",
			       argv[i]);
			return 1;

		} else {

			break;

		}
	}

	if (i != (argc - 1))
		return zbc_info_usage(argv[0]);

	/* Open device */
	ret = zbc_device_is_zoned(argv[i], do_fake, &info);
	if (ret == 1) {
		printf("Device %s:\n", argv[i]);
		zbc_print_device_info(&info, stdout);
		ret = 0;
	} else if (ret == 0) {
		printf("%s is not a zoned block device\n", argv[i]);
	} else {
		fprintf(stderr,
			"zbc_device_is_zoned failed %d (%s)\n",
			ret, strerror(-ret));
		ret = 1;
	}

	return ret;
}
