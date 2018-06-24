/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Author: Masato Suzuki (masato.suzuki@wdc.com)
 *         Damien Le Moal (damien.lemoal@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int flags = 0;
	unsigned int oflags;
	long long lba;
	char *path;
	int ret;

	/* Check command line */
	if (argc < 3 || argc > 4) {
		printf("Usage: %s [-v] <dev> <lba>\n"
		       "  If lba is -1, then finish all zones\n"
		       "Options:\n"
		       "  -v : Verbose mode\n",
		       argv[0]);
		return 1;
	}

	if (argc == 4) {
		if (strcmp(argv[1], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[1], "--ALL") == 0) {
			flags = ZBC_OP_ALL_ZONES;
			zbc_set_log_level("debug");
		} else {
			printf("Unknown option \"%s\"\n", argv[1]);
			return 1;
		}
		path = argv[2];
		lba = atoll(argv[3]);
	} else {
		path = argv[1];
		lba = atoll(argv[2]);
	}

	/* Open device */
	oflags = ZBC_O_DEVTEST;
	oflags |= ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(path, oflags | O_RDWR, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed, err %d (%s) %s\n",
			ret, strerror(-ret), path);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	if (lba == -1) {
		flags = ZBC_OP_ALL_ZONES;
		lba = 0;
	}

	/* Finish zone(s) */
	ret = zbc_finish_zone(dev, zbc_lba2sect(&info, lba), flags);
	if (ret != 0) {
		struct zbc_errno zbc_err;
		const char *sk_name;
		const char *ascq_name;

		fprintf(stderr,
			"[TEST][ERROR],zbc_test_finish_zone failed %d\n",
			ret);

		zbc_errno(dev, &zbc_err);
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);

		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
		ret = 1;
	}

	/* Close device file */
	zbc_close(dev);

	return ret;
}

