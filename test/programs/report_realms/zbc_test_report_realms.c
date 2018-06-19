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
	struct zbc_zone_realm *r, *realms = NULL;
	unsigned int nr_realms, oflags;
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
		fprintf(stderr, "[TEST][ERROR],open device failed, err %d (%s) %s\n",
			ret, strerror(-ret), argv[i]);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	/* Get the number of zone realms */
	ret = zbc_report_nr_realms(dev, &nr_realms);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_report_nr_realms failed %d\n",
			ret);
		goto out;
	}

	/* Allocate zone realm descriptor array */
	realms = (struct zbc_zone_realm *)calloc(nr_realms,
						  sizeof(struct zbc_zone_realm));
	if (!realms) {
		fprintf(stderr,
			"[TEST][ERROR],No memory\n");
		ret = 1;
		goto out;
	}

	/* Get zone realm information */
	ret = zbc_report_realms(dev, realms, &nr_realms);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_report_realms failed %d\n",
			ret);
		goto out;
	}

	for (i = 0; i < (int)nr_realms; i++) {
		r = &realms[i];
		printf("[ZONE_REALM_INFO],%03d,0x%x,%08llu,%u,%08llu,%u,%u,%s,%s\n",
		       zbc_zone_realm_number(r),
		       zbc_zone_realm_type(r),
		       zbc_sect2lba(&info, zbc_zone_realm_conv_start(r)),
		       zbc_sect2lba(&info, zbc_zone_realm_conv_length(r)),
		       zbc_sect2lba(&info, zbc_zone_realm_seq_start(r)),
		       zbc_sect2lba(&info, zbc_zone_realm_seq_length(r)),
		       zbc_zone_realm_keep_out(r),
		       zbc_zone_realm_to_conv(r) ? "Y" : "N",
		       zbc_zone_realm_to_seq(r) ? "Y" : "N");
	}

out:
	if (ret && ret != 1) {
		struct zbc_err_ext zbc_err;
		const char *sk_name;
		const char *ascq_name;
		uint64_t err_cbf;
		uint16_t err_za;

		zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
		err_za = zbc_err.err_za;
		err_cbf = zbc_err.err_cbf;

		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
		if (err_za || err_cbf) {
			printf("[TEST][ERROR][ERR_ZA],0x%04x\n", err_za);
			printf("[TEST][ERROR][ERR_CBF],%lu\n", err_cbf);
		}
		ret = 1;
	}

	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

