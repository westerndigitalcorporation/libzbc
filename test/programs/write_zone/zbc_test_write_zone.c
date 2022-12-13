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

#include "libzbc/zbc.h"
#include "zbc_private.h"

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	char *path, *end;
	void *iobuf = NULL;
	struct iovec *iov = NULL;
	size_t bufsize, iosize;
	ssize_t ret;
	unsigned long long lba, sector;
	unsigned int oflags, lba_count, sector_count;
	int i, nio = 1, pattern = 0, iovcnt = 1, n;
	bool vio = false;

	/* Check command line */
	if (argc < 4) {
usage:
		printf("Usage: %s [options] <dev> <lba> <num lba>\n"
		       "  Write <num LBA> LBAs from LBA <lba>\n"
		       "Options:\n"
		       "  -v	     : Verbose mode\n"
		       "  -vio <num> : Use vectored I/Os with <num> buffers\n"
		       "               of <I/O size> bytes, resulting in effective\n"
		       "               I/O size of <num> x <I/O size> B\n"
		       "  -p <num>   : Set the byte pattern to write. If this option\n"
		       "               is omitted, write data buffer is not initialized"
		       "  -n <nio>   : Repeat sequentially the write operation <nio> times\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < argc - 3; i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-p") == 0) {

			if (i >= (argc - 1))
				goto usage;
			i++;

			pattern = strtol(argv[i], &end, 0);
			if (*end != '\0' || errno != 0) {
				fprintf(stderr,
					"Invalid data pattern value \"%s\"\n",
					argv[i]);
				goto usage;
			}
			if (pattern > 0xff) {
				fprintf(stderr,
					"Not a single-byte pattern:\"%s\"\n",
					argv[i]);
				goto usage;
			}

		} else if (strcmp(argv[i], "-vio") == 0) {

			if (i >= (argc - 1))
				goto usage;
			i++;

			iovcnt = atoi(argv[i]);
			if (iovcnt <= 0) {
				fprintf(stderr,
					"Invalid number of VIO buffers\n");
				goto usage;
			}
			vio = true;

		} else if (strcmp(argv[i], "-n") == 0) {

			if (i >= argc - 1)
				goto usage;
			i++;

			nio = atoi(argv[i]);
			if (nio <= 0) {
				fprintf(stderr, "Invalid number of I/O\n");
				goto usage;
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
	oflags = ZBC_O_DEVTEST;
	oflags |= ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(path, oflags | O_WRONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed, err %zd (%s) %s\n",
			ret, strerror(-ret), path);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);
	sector = zbc_lba2sect(&info, lba);
	sector_count = zbc_lba2sect(&info, lba_count);

	bufsize = lba_count * info.zbd_lblock_size;

	if (vio) {
		iov = calloc(iovcnt, sizeof(struct iovec));
		if (!iov) {
			fprintf(stderr, "No memory for I/O vector\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	iosize = bufsize * iovcnt;
	ret = posix_memalign((void **) &iobuf, info.zbd_lblock_size, iosize);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],No memory for I/O buffer (%zu B)\n",
			iosize);
		ret = 1;
		goto out;
	}

	/* Don't send uninitialized bytes to syscall */
	memset(iobuf, pattern, iosize);

	while (nio) {

		if (vio) {
			n = zbc_map_iov(iobuf, sector_count,
					iov, iovcnt, bufsize >> 9);
			if (n < 0) {
				fprintf(stderr,
					"[TEST][ERROR],iov map failed %d\n",
					-n);
				ret = 1;
				goto out;
			}
			ret = zbc_pwritev(dev, iov, n, sector);
		} else {
			ret = zbc_pwrite(dev, iobuf, sector_count, sector);
		}
		if (ret <= 0) {
			struct zbc_errno zbc_err;
			const char *sk_name;
			const char *ascq_name;

			fprintf(stderr,
				"[TEST][ERROR],zbc_write_zone failed %zd"
				" sector_count=%u\n",
				ret, sector_count);

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
	free(iobuf);
	zbc_close(dev);
	free(iov);

	return ret;
}

