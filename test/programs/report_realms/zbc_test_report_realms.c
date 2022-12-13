/*
 * SPDX-License-Identifier: BSD-2-Clause
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Copyright (c) 2023 Western Digital Corporation or its affiliates.
 *
 * This file is part of libzbc.
 *
 * Author: Dmitry Fomichev (dmitry.fomichev@wdc.com)
 */

#include <inttypes.h>
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
	unsigned long long sector = 0LL;
	unsigned int j, nr_realms, oflags;
	int i, ret = 1, lba_units = 1;
	enum zbc_realm_report_options ro = ZBC_RR_RO_ALL;

	/* Check command line */
	if (argc < 2) {
usage:
		fprintf(stderr,
			"Usage: %s [options] <dev>\n"
			"Options:\n"
			"  -v		: Verbose mode\n"
			"  -sector      : Use 512B sector block addresses (logical block units are used by default)\n"
			"  -ro          : Realm reporting options:\n"
			"               :   all     - Report all realms (default)\n"
			"               :   sobr    - Report all realms that contain active SOBR zones\n"
			"               :   seq     - Report all realms that contain active SWR zones\n"
			"               :   seqp    - Report all realms that contain active SWP zones\n"
			"               :   invalid - Send a reporting option value that is known to be invalid\n"
			"  -start       : Realm locator LBA/sector (0 by default)\n",
			argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-ro") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "all") == 0) {
				ro = ZBC_RR_RO_ALL;
			} else if (strcmp(argv[i], "sobr") == 0) {
				ro = ZBC_RR_RO_SOBR;
			} else if (strcmp(argv[i], "seq") == 0) {
				ro = ZBC_RR_RO_SWR;
			} else if (strcmp(argv[i], "seqp") == 0) {
				ro = ZBC_RR_RO_SWP;
			} else if (strcmp(argv[i], "invalid") == 0) {
				ro = 0x15;
			} else {
				fprintf(stderr, "Unknown reporting option \"%s\"\n",
					argv[i]);
				goto usage;
			}
		} else if (strcmp(argv[i], "-start") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			sector = strtoll(argv[i], NULL, 10);
		} else if (strcmp(argv[i], "-sector") == 0) {
			lba_units = 1;
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

	if (lba_units)
		sector = zbc_lba2sect(&info, sector);

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
	ret = zbc_report_realms(dev, sector, ro, realms, &nr_realms);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_report_realms failed %d\n",
			ret);
		goto out;
	}

	for (i = 0, r = realms; i < (int)nr_realms; i++, r++) {
		printf("[ZONE_REALM_INFO],%d,%u,0x%x,0x%x,%s,%s,0x%x,%s,%s,%u,;",
		zbc_zone_realm_number(r),
		zbc_zone_realm_domain(r),
		zbc_zone_realm_type(r),
		zbc_zone_realm_restrictions(r),
		zbc_realm_activation_allowed(r) ? "Y" : "N",
		zbc_realm_wp_reset_allowed(r) ? "Y" : "N",
		zbc_zone_realm_actv_flags(r),
		zbc_zone_realm_actv_as_conv(r) ? "Y" : "N",
		zbc_zone_realm_actv_as_seq(r) ? "Y" : "N",
		zbc_zone_realm_nr_domains(r));

		for (j = 0; j < zbc_zone_realm_nr_domains(r); j++) {
			printf("%u:%"PRIu64":%"PRIu64":%u",
			zbc_realm_zone_type(r, j),
			lba_units ? zbc_realm_start_lba(dev, r, j) :
				    zbc_realm_start_sector(r, j),
			lba_units ? zbc_realm_end_lba(dev, r, j) :
				    zbc_realm_high_sector(dev, r, j),
			zbc_realm_length(r, j));

			printf(j == zbc_zone_realm_nr_domains(r) - 1 ? "\n" : ";");
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
			printf("[TEST][ERROR][ERR_CBF],%"PRIu64"\n", err_cbf);
		}
		ret = 1;
	}

	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

