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
#include <errno.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	struct zbc_cvt_range *r, *ranges = NULL;
	unsigned int nr_ranges, oflags;
	int i, ret = 1;

	/* Check command line */
	if (argc < 2) {
usage:
		fprintf(stderr,
			"Usage: %s [options] <dev>\n"
			"Options:\n"
			"    -v         : Verbose mode\n",
			argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (argv[i][0] == '-') {

			printf("Unknown option \"%s\"\n", argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (i != argc - 1)
		goto usage;

	/* Open device */
	oflags = ZBC_O_DEVTEST;
	oflags |= ZBC_O_DRV_ATA;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(argv[i], oflags | O_RDONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed %d\n",
			ret);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	/* Get the number of conversion ranges */
	ret = zbc_media_report_nr_ranges(dev, &nr_ranges);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],zbc_media_report_nr_ranges failed %d\n",
			ret);
		return 1;
	}

	/* Allocate conversion range descriptor array */
	ranges = (struct zbc_cvt_range *)calloc(nr_ranges, sizeof(struct zbc_cvt_range));
	if (!ranges) {
		fprintf(stderr,
			"[TEST][ERROR],No memory\n");
		return 1;
	}

	/* Get conversion range information */
	ret = zbc_media_report(dev, ranges, &nr_ranges);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_media_report failed %d\n",
			ret);
		return 1;
	}

	for (i = 0; i < (int)nr_ranges; i++) {
		r = &ranges[i];
		printf("[CVT_RANGE_INFO],%03d,0x%x,%08llu,%u,%08llu,%u,%u,%s,%s\n",
		       zbc_cvt_range_number(r),
		       zbc_cvt_range_type(r),
		       zbc_sect2lba(&info, zbc_cvt_range_conv_start(r)),
		       zbc_sect2lba(&info, zbc_cvt_range_conv_length(r)),
		       zbc_sect2lba(&info, zbc_cvt_range_seq_start(r)),
		       zbc_sect2lba(&info, zbc_cvt_range_seq_length(r)),
		       zbc_cvt_range_keep_out(r),
		       zbc_cvt_range_to_conv(r) ? "Y" : "N",
		       zbc_cvt_range_to_seq(r) ? "Y" : "N");
        }

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

	if (ranges)
		free(ranges);
	zbc_close(dev);

	return ret;
}

