/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2018, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Author: Dmitry Fomichev (dmitry.fomichev@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	uint64_t start_realm;
	unsigned int oflags, nr_realms;
	int i, type, fg = 0, ret;
	char *path;

	/* Check command line */
	if (argc < 5) {
		printf("Usage: %s [options] <dev> <start realm> "
		       "<num realms> <conv | seq> [<fg>]\n"
		       "Options:\n"
		       "    -v            : Verbose mode\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (strcmp(argv[i], "-v") == 0)
			zbc_set_log_level("debug");
		else if ( argv[i][0] == '-' ) {
			fprintf(stderr,
				"[TEST][ERROR],Unknown option \"%s\"\n",
				argv[i]);
			return 1;
		}
		else
			break;
	}

	/* Get parameters */
	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing zoned device path\n");
		return 1;
	}
	path = argv[i++];

	if (i >= argc) {
		fprintf(stderr,
			"[TEST][ERROR],Missing starting realm number\n");
		return 1;
	}
	start_realm = atol(argv[i++]);

	if (i >= argc) {
		fprintf(stderr,
			"[TEST][ERROR],Number of realms to convert missing\n");
		return 1;
	}
	nr_realms = atoi(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing new zone type\n");
		return 1;
	}
	if (strcmp(argv[i++], "conv") == 0) {
		type = ZBC_ZT_CONVENTIONAL;
	} else if (strcmp(argv[i], "seq") == 0) {
		type = ZBC_ZT_SEQUENTIAL_REQ;
	} else {
		fprintf(stderr, "[TEST][ERROR],Invalid new zone type\n");
		return 1;
	}

	if (i < argc)
		fg = atoi(argv[i]);

	/* Open device */
	oflags = ZBC_O_DEVTEST | ZBC_O_DRV_ATA;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(path, oflags | O_WRONLY, &dev);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],open device failed, err %i (%s)\n",
			ret, strerror(-ret));
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	/* Convert realms */
	ret = zbc_convert_realms(dev, start_realm, nr_realms, type, fg);
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

	zbc_close(dev);

	return ret;
}

