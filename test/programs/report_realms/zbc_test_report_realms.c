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
	struct zbc_zone *zones = NULL;
	uint64_t lba;
	unsigned int j, nr_realms, oflags, len;
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

	/* Allocate zone array */
	zones = (struct zbc_zone *)calloc(1, sizeof(struct zbc_zone));
	if (!zones) {
		fprintf(stderr, "[TEST][ERROR],No memory\n");
		ret = 1;
		goto out;
	}

	/*
	 * Get information about the first zone of every realm
	 * and calculate the size of the realm in zones.
	 */
	for (i = 0; i < (int)nr_realms; i++) {
		r = &realms[i];
		for (j = 0; j < zbc_zone_realm_nr_domains(r); j++) {
			if (!zbc_realm_actv_as_dom_id(r, j))
				continue;
			lba = zbc_realm_start_lba(r, j);
			ret = zbc_report_zones(dev, lba, 0, zones, &len);
			if (ret != 0) {
				fprintf(stderr, "zbc_report_zones failed %d\n", ret);
				ret = 1;
				goto out;
			}
			if (!len || zones->zbz_start != lba || zones->zbz_length == 0) {
				fprintf(stderr,
					"malformed zone response, nz=%u, start=%lu, len=%lu\n",
					len, zones->zbz_start, zones->zbz_length);
				ret = 1;
				goto out;
			}

			len = zbc_realm_block_length(r, j) / zones->zbz_length;
			r->zbr_ri[j].zbi_length = len;
		}
	}

	for (i = 0, r = realms; i < (int)nr_realms; i++, r++) {
		printf("[ZONE_REALM_INFO],%03d:,%u,0x%x,0x%x;",
		zbc_zone_realm_number(r),
		zbc_zone_realm_domain(r),
		zbc_zone_realm_type(r),
		zbc_zone_realm_actv_flags(r));

		for (j = 0; j < zbc_zone_realm_nr_domains(r); j++) {
			printf("%u:%lu,%lu,%u",
			zbc_realm_zone_type(r, j),
			zbc_sect2lba(&info, zbc_realm_start_lba(r, j)),
			zbc_sect2lba(&info, zbc_realm_end_lba(r, j)),
			zbc_realm_length(r, j));
			printf(j == zbc_zone_realm_nr_domains(r) - 1 ? "\n" : "; ");
		}
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
	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;
}

