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
	struct zbc_zone_domain *domains = NULL, *d;
	struct zbc_zone_realm *realms = NULL, *r;
	struct zbc_realm_item *ri;
	struct zbc_actv_res *actv_recs = NULL, *cr;
	const char *sk_name, *ascq_name;
	char *path;
	struct zbc_device_info info;
	struct zbc_zp_dev_control ctl;
	struct zbc_err_ext zbc_err;
	uint64_t start;
	uint64_t err_cbf;
	uint16_t err_za;
	unsigned int oflags, nr_units, nr_realms, new_type, nr_actv_recs = 0;
	unsigned int nr_domains, domain_id;
	int i, ret, end;
	bool no_query = false, zone_addr = false, all = false, cdb32 = false, fsnoz = false;

	/* Check command line */
	if (argc < 5) {
		printf("Usage: %s [options] <dev> <start zone realm> <num realms> <conv|seq>\n"
		       "or\n%s -z [options] <dev> <start zone LBA> <num zones> <conv|seq|sobr|seqp>\n"
		       "Options:\n"
		       "    -a            : Try to activate all, even if not every zone can be\n"
		       "    -32           : Force using 32-byte SCSI command (16 by default)\n"
		       "    -n            : Set the number of zones to activate via FSNOZ\n"
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
			zone_addr ? "zone LBA" : "zone realm");
		return 1;
	}
	start = atol(argv[i++]);

	if (i >= argc) {
		fprintf(stderr,
			"[TEST][ERROR],Missing number of %ss to activate\n",
			zone_addr ? "zone" : "zone realm");
		return 1;
	}
	nr_units = atoi(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing new zone type\n");
		return 1;
	}
	if (strcmp(argv[i], "conv") == 0)
		new_type = ZBC_ZT_CONVENTIONAL;
	else if (strcmp(argv[i], "sobr") == 0)
		new_type = ZBC_ZT_SEQ_OR_BEF_REQ;
	else if (strcmp(argv[i], "seq") == 0)
		new_type = ZBC_ZT_SEQUENTIAL_REQ;
	else if (strcmp(argv[i], "seqp") == 0)
		new_type = ZBC_ZT_SEQUENTIAL_PREF;
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
			"[TEST][ERROR],open device failed, err %i (%s) %s\n",
			ret, strerror(-ret), path);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	if (!(info.zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT)) {
		fprintf(stderr,
			"[TEST][ERROR],not a Zone Domains device\n");
		ret = 1;
		goto out;
	}
	if (!(info.zbd_flags & ZBC_ZONE_QUERY_SUPPORT))
		no_query = true;

	if (!zone_addr || no_query) {
		/*
		 * Have to call zbc_list_zone_realms() to find the
		 * starting zone and number of zones to activate.
		 */
		ret = zbc_list_zone_realms(dev, &realms, &nr_realms);
		if (ret != 0) {
			zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			fprintf(stderr,
				"[TEST][ERROR],zbc_list_zone_realms failed, err %i (%s)\n",
				ret, strerror(-ret));
			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			ret = 1;
			goto out;
		}
		if (no_query)
			nr_actv_recs = nr_realms << 1;

		if (!zone_addr) {
			if (start + nr_units > nr_realms) {
				fprintf(stderr,
					"[TEST][ERROR],Realm [%lu/%u] out of range\n",
					start, nr_units);
				ret = 1;
				goto out;
			}
			end = start + nr_units;

			/* Find domain ID for the new zone type */
			r = &realms[start];
			ri = zbc_realm_item_by_type(r, new_type);
			if (ri == NULL) {
				fprintf(stderr,
					"[TEST][ERROR],Realm #%lu doesn't support zone type %u\n",
					start, new_type);
				ret = 1;
				goto out;
			}
			domain_id = ri->zbi_dom_id;

			/* Set the start LBA and the length in zones */
			for (nr_units = 0, i = start; i < end; i++)
				nr_units += zbc_realm_length(&realms[i], domain_id);

			start = zbc_realm_start_lba(r, domain_id);
		} else {
			/* Find domain ID for the new zone type */
			ret = zbc_list_domains(dev, &domains, &nr_domains);
			if (ret != 0) {
				fprintf(stderr,
					"[TEST][ERROR],zbc_list_domains failed, err %i (%s) %s\n",
					ret, strerror(-ret), path);
				ret = 1;
				goto out;
			}

			for (i = 0, d = domains; i < (int)nr_domains; i++, d++) {
				if (d->zbm_type == new_type) {
					domain_id = i;
					break;
				}
			}
			if (i >= (int)nr_domains) {
				fprintf(stderr,
					"[TEST][ERROR],Device doesn't support zone type %u\n",
					new_type);
				ret = 1;
				goto out;
			}
		}
	}

	if (cdb32)
		fsnoz = false;

	if (fsnoz) {
		/* Make sure the device supports this */
		if (!(info.zbd_flags & ZBC_ZA_CONTROL_SUPPORT)) {
			fprintf(stderr,
				"[TEST][ERROR],device doesn't support setting FSNOZ\n");
			ret = 1;
			goto out;
		}

		/* Set the number of zones to activate via a separate command */
		ctl.zbm_nr_zones = nr_units;
		ctl.zbm_urswrz = 0xff;
		ctl.zbm_max_activate = 0xffff;
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
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

	if (!no_query) {
		ret = zbc_get_nr_actv_records(dev, !fsnoz, all, cdb32, start,
					      nr_units, new_type);
		if (ret < 0) {
			zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			err_za = zbc_err.err_za;
			err_cbf = zbc_err.err_cbf;
			fprintf(stderr,
				"[TEST][ERROR],Can't get the number of activation records, err %i (%s)\n",
				ret, strerror(-ret));
			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			if (err_za || err_cbf) {
				printf("[TEST][ERROR][ERR_ZA],0x%04x\n", err_za);
				printf("[TEST][ERROR][ERR_CBF],%lu\n", err_cbf);
			}
			ret = 1;
			goto out;
		}
		nr_actv_recs = (uint32_t)ret;
	}

	/* Allocate activation results record array */
	actv_recs = (struct zbc_actv_res *)calloc(nr_actv_recs,
						  sizeof(struct zbc_actv_res));
	if (!actv_recs) {
		fprintf(stderr, "[TEST][ERROR],No memory\n");
		goto out;
	}

	/* Activate zones */
	ret = zbc_zone_activate(dev, !fsnoz, all, cdb32, start, nr_units,
				new_type, actv_recs, &nr_actv_recs);
	if (ret != 0) {
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
	}
	for (i = 0; i < (int)nr_actv_recs; i++) {
		cr = &actv_recs[i];
		printf("[ACTV_RECORD],%lu,%u,%x,%x\n",
		       cr->zbe_start_zone,
		       cr->zbe_nr_zones,
		       cr->zbe_type,
		       cr->zbe_condition);
	}

out:
	if (domains)
		free(domains);
	if (realms)
		free(realms);
	if (actv_recs)
		free(actv_recs);
	zbc_close(dev);

	return ret;
}

