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
#include <unistd.h>
#include <string.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	size_t iosize;
	void *iobuf = NULL;
	unsigned long long lba;
	unsigned int lba_count;
	unsigned long long sector;
	unsigned int sector_count;
	int i, nio = 1;
	ssize_t ret;
	char *path;

	/* Check command line */
	if (argc < 4) {
usage:
		printf("Usage: %s [options] <dev> <lba> <num lba>\n"
		       "  Write <num LBA> LBAs from LBA <lba>\n"
		       "Options:\n"
		       "  -v	   : Verbose mode\n"
		       "  -n <nio> : Repeat sequentially the write operation <nio> times\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < argc - 3; i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-n") == 0) {

			i++;
			if (i >= argc - 1)
				goto usage;

			nio = atoi(argv[i]);
			if (nio <= 0) {
				fprintf(stderr, "Invalid number of I/O\n");
				return 1;
			}

		} else if (argv[i][0] == '-') {

			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (i != argc - 3)
		goto usage;

	/* Get parameters */
	path = argv[i];
	lba = atoll(argv[i+1]);
	lba_count = (uint32_t)atoi(argv[i+2]);

	/* Open device */
	ret = zbc_open(path, O_WRONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed %zd\n",
			ret);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_set_test_mode(dev);
	zbc_get_device_info(dev, &info);
	sector = zbc_lba2sect(&info, lba);
	sector_count = zbc_lba2sect(&info, lba_count);

	iosize = lba_count * info.zbd_lblock_size;
	ret = posix_memalign((void **) &iobuf, info.zbd_lblock_size, iosize);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],No memory for I/O buffer (%zu B)\n",
			iosize);
		ret = 1;
		goto out;
	}

	while (nio) {

		ret = zbc_pwrite(dev, iobuf, sector_count, sector);
		if (ret <= 0) {
			struct zbc_errno zbc_err;
			const char *sk_name;
			const char *ascq_name;

			fprintf(stderr,
				"[TEST][ERROR],zbc_write_zone failed %zd\n",
				ret);

			zbc_errno(dev, &zbc_err);
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);

			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			ret = 1;
			break;

		}

		nio--;
		sector += sector_count;
		ret = 0;

	}

out:
	if (iobuf)
		free(iobuf);
	zbc_close(dev);

	return ret;
}

