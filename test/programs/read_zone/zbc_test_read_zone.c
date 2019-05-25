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
	void *iobuf = NULL;
	struct iovec *iov = NULL;
	char *path, *end;
	unsigned char *b;
	size_t bufsize, iosize;
	ssize_t ret;
	unsigned long long lba, sector, start_sect;
	unsigned int oflags, lba_count, sector_count;
	int i, nio = 1, pattern = 0, iovcnt = 1, n;
	bool vio = false, ptrn_set = false;

	/* Check command line */
	if (argc < 4) {
usage:
		printf("Usage: %s [-v] <dev> <lba> <num lba>\n"
		       "  Read <num LBA> LBAs from LBA <lba>\n"
		       "Options:\n"
		       "  -v         : Verbose mode\n"
		       "  -vio <num> : Use vectored I/Os with <num> buffers\n"
		       "               of <I/O size> bytes, resulting in effective\n"
		       "               I/O size of <num> x <I/O size> B\n"
		       "  -p <num>   : Expect all bytes that are read to have\n"
		       "               the value <num>. If there is a mismatch,\n"
		       "               the program will output it's data offset\n"
		       "  -n <nio>   : Repeat sequentially the read operation <nio> times\n",
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
			ptrn_set = true;

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

	ret = zbc_open(path, oflags | O_RDONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed %zd\n",
			ret);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);
	sector = zbc_lba2sect(&info, lba);
	start_sect = sector;
	sector_count = zbc_lba2sect(&info, lba_count);

	if (vio) {
		iov = calloc(iovcnt, sizeof(struct iovec));
		if (!iov) {
			fprintf(stderr, "No memory for I/O vector\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	bufsize = lba_count * info.zbd_lblock_size;
	iosize = bufsize * iovcnt;
	ret = posix_memalign((void **) &iobuf, info.zbd_lblock_size, iosize);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],No memory for I/O buffer (%zu B)\n",
			iosize);
		ret = 1;
		goto out;
	}

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
			ret = zbc_preadv(dev, iov, n, sector);
		} else {
			ret = zbc_pread(dev, iobuf, sector_count, sector);
		}
		if (ret <= 0) {
			struct zbc_errno zbc_err;
			const char *sk_name;
			const char *ascq_name;

			fprintf(stderr,
				"[TEST][ERROR],zbc_read_zone failed %zd,"
				" sector=%llu, sector_count=%u\n",
				ret, sector, sector_count);

			zbc_errno(dev, &zbc_err);
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);

			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			ret = 1;
			goto out;
		}

		if (ptrn_set) {
			for (i = 0, b = iobuf; i < (ret << 9); i++, b++) {
				if (*b != (unsigned char)pattern) {
					unsigned long long err_sect = start_sect + (i >> 9);
					unsigned long long err_ofs = (start_sect << 9) + i - (err_sect << 9);
					fprintf(stderr,
						"[TEST][ERROR],Data mismatch @ sector %llu / offset %llu: read %#x, exp %#x\n",
						err_sect, err_ofs, *b, pattern);
					ret = ERANGE;
					break;
				}
			}
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

