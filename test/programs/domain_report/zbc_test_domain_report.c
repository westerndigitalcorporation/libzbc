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
	struct zbc_cvt_domain *d, *domains = NULL;
	unsigned int nr_domains, oflags;
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

	/* Get the number of conversion domains */
	ret = zbc_report_nr_domains(dev, &nr_domains);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_report_nr_domains failed %d\n",
			ret);
		goto out;
	}

	/* Allocate conversion domain descriptor array */
	domains = (struct zbc_cvt_domain *)calloc(nr_domains,
						  sizeof(struct zbc_cvt_domain));
	if (!domains) {
		fprintf(stderr,
			"[TEST][ERROR],No memory\n");
		ret = 1;
		goto out;
	}

	/* Get conversion domain information */
	ret = zbc_domain_report(dev, domains, &nr_domains);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_domain_report failed %d\n",
			ret);
		goto out;
	}

	for (i = 0; i < (int)nr_domains; i++) {
		d = &domains[i];
		printf("[CVT_DOMAIN_INFO],%03d,0x%x,%08llu,%u,%08llu,%u,%u,%s,%s\n",
		       zbc_cvt_domain_number(d),
		       zbc_cvt_domain_type(d),
		       zbc_sect2lba(&info, zbc_cvt_domain_conv_start(d)),
		       zbc_sect2lba(&info, zbc_cvt_domain_conv_length(d)),
		       zbc_sect2lba(&info, zbc_cvt_domain_seq_start(d)),
		       zbc_sect2lba(&info, zbc_cvt_domain_seq_length(d)),
		       zbc_cvt_domain_keep_out(d),
		       zbc_cvt_domain_to_conv(d) ? "Y" : "N",
		       zbc_cvt_domain_to_seq(d) ? "Y" : "N");
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

	if (domains)
		free(domains);
	zbc_close(dev);

	return ret;
}

