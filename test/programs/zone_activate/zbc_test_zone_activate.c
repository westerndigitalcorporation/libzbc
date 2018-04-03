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
	struct zbc_conv_rec *conv_recs = NULL, *cr;
	struct zbc_cvt_domain *domains = NULL;
	const char *sk_name, *ascq_name;
	struct zbc_zp_dev_control ctl;
	char *path;
	struct zbc_errno zbc_err;
	uint64_t start;
	unsigned int oflags, nr_units, nr_domains, new_type, nr_conv_recs = 0;
	int i, ret, end;
	bool zone_addr = false, all = false, cdb32 = false, fsnoz = false;

	/* Check command line */
	if (argc < 5) {
		printf("Usage: %s [options] <dev> <start conversion domain> <num domains> <conv|seq>\n"
		       "or\n%s -z [options] <dev> <start zone LBA> <num zones> <conv|seq>\n"
		       "Options:\n"
		       "    -a            : Try to convert all, even if not every zone can be\n"
		       "    -32           : Force using 32-byte SCSI command (16 by default)\n"
		       "    -n            : Set the number of zones to convert via FSNOZ\n"
		       "    -v            : Verbose mode\n",
		       argv[0], argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0)
			zbc_set_log_level("debug");
		else if (strcmp(argv[i], "-a") == 0)
			all = true;
		else if (strcmp(argv[i], "-z") == 0)
			zone_addr = true;
		else if (strcmp(argv[i], "-32") == 0)
			cdb32 = true;
		else if (strcmp(argv[i], "-n") == 0)
			fsnoz = true;
		else {
			fprintf(stderr,
				"[TEST][ERROR],Unknown option \"%s\"\n",
				argv[i]);
			return 1;
		}
	}

	/* Get parameters */
	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing zoned device path\n");
		return 1;
	}
	path = argv[i++];

	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing starting %s\n",
			zone_addr ? "zone LBA" : "conversion domain");
		return 1;
	}
	start = atol(argv[i++]);

	if (i >= argc) {
		fprintf(stderr,
			"[TEST][ERROR],Missing number of %ss to convert\n",
			zone_addr ? "zone" : "conevrsion domain");
		return 1;
	}
	nr_units = atoi(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing new zone type\n");
		return 1;
	}
	if (strcmp(argv[i], "conv") == 0)
		new_type = ZBC_ZT_CONVENTIONAL;
	else if (strcmp(argv[i], "seq") == 0)
		new_type = ZBC_ZT_SEQUENTIAL_REQ;
	else {
		fprintf(stderr, "[TEST][ERROR],Invalid new zone type\n");
		return 1;
	}

	if (++i < argc) {
		fprintf(stderr, "[TEST][ERROR],Extra argument '%s'\n",
			argv[i]);
		return 1;
	}

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

	if (!zone_addr) {
		/*
		 * Have to call zbc_list_conv_domains() to find the
		 * starting zone and number of zones to convert.
		 */
		ret = zbc_list_conv_domains(dev, &domains, &nr_domains);
		if (ret != 0) {
			zbc_errno(dev, &zbc_err);
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			fprintf(stderr,
				"[TEST][ERROR],zbc_list_conv_domains failed, err %i (%s)\n",
				ret, strerror(-ret));
			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			ret = 1;
			goto out;
		}

		if (start + nr_units > nr_domains) {
			fprintf(stderr,
				"[TEST][ERROR],Domain [%lu/%u] out of range\n",
				start, nr_units);
			ret = 1;
			goto out;
		}
		end = start + nr_units;
		if (new_type == ZBC_ZT_CONVENTIONAL) {
			for (nr_units = 0, i = start; i < end; i++)
				nr_units += domains[i].zbr_seq_length;
			start = domains[start].zbr_seq_start;
		} else {
			for (nr_units = 0, i = start; i < end; i++)
				nr_units += domains[i].zbr_conv_length;
			start = domains[start].zbr_conv_start;
		}
	}

	if (cdb32)
		fsnoz = false;

	if (fsnoz) {
		/* Set the number of zones to convert via a separate command */
		ctl.zbm_nr_zones = nr_units;
		ctl.zbm_cmr_wp_check = 0xff;
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			zbc_errno(dev, &zbc_err);
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			fprintf(stderr, "Can't set FSNOZ, err %i (%s)\n",
				ret, strerror(-ret));
			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			ret = 1;
			goto out;
		}
		nr_units = 0;
	}

	ret = zbc_get_nr_cvt_records(dev, all, cdb32, start,
				     nr_units, new_type);
	if (ret < 0) {
		zbc_errno(dev, &zbc_err);
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
		fprintf(stderr,
			"[TEST][ERROR],Can't get the number of conversion records, err %i (%s)\n",
			ret, strerror(-ret));
		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
		ret = 1;
		goto out;
	}
	nr_conv_recs = (uint32_t)ret;

	/* Allocate conversion record array */
	conv_recs = (struct zbc_conv_rec *)calloc(nr_conv_recs,
						  sizeof(struct zbc_conv_rec));
	if (!conv_recs) {
		fprintf(stderr, "[TEST][ERROR],No memory\n");
		goto out;
	}

	/* Convert zones */
	ret = zbc_zone_activate(dev, all, cdb32, start, nr_units,
				new_type, conv_recs, &nr_conv_recs);
	if (ret != 0) {
		zbc_errno(dev, &zbc_err);
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
	}
	for (i = 0; i < (int)nr_conv_recs; i++) {
		cr = &conv_recs[i];
		printf("[CVT_RECORD],%lu,%u,%x,%x\n",
		       cr->zbe_start_zone,
		       cr->zbe_nr_zones,
		       cr->zbe_type,
		       cr->zbe_condition);
	}

out:
	if (domains)
		free(domains);
	if (conv_recs)
		free(conv_recs);
	zbc_close(dev);

	return ret;
}

