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
#include <ctype.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

struct cmd_options {
	uint64_t start;
	unsigned int nr_units;
	unsigned int new_type;
	unsigned int domain_id;
	bool lba_units;
	bool query;
	bool fsnoz;
	bool verbose;
	bool all;
	bool zone_addr;
	bool cdb32;
	bool reset;
	bool skip_same;
};

static int chunk_activation(struct zbc_device *dev, struct zbc_device_info *info,
                            struct cmd_options *opts, uint64_t start_zone,
			    unsigned int nr_zones, unsigned int domain_id,
			    uint64_t reset_start, unsigned int reset_zones)
{
	struct zbc_actv_res *actv_recs = NULL, *cr;
	struct zbc_zd_dev_control ctl;
	struct zbc_err_ext zbc_err;
	const char *sk_name, *ascq_name;
	uint64_t err_cbf;
	uint16_t err_za;
	unsigned int nr_actv_recs;
	int ret = 0, i;

	if (opts->fsnoz) {
		/* Make sure the device supports this */
		if (!(info->zbd_flags & ZBC_ZA_CONTROL_SUPPORT)) {
			fprintf(stderr,
				"[TEST][ERROR],device doesn't support setting FSNOZ\n");
			ret = 1;
			goto out;
		}

		/* Set the number of zones to activate via a separate command */
		ctl.zbt_nr_zones = nr_zones;
		ctl.zbt_urswrz = 0xff;
		ctl.zbt_max_activate = 0xffff;
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

		/* Read back to make sure the value is set correctly */
		ret = zbc_zone_activation_ctl(dev, &ctl, false);
		if (ret != 0) {
			zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			fprintf(stderr, "Can't read FSNOZ, err %i (%s)\n",
				ret, strerror(-ret));
			printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
			printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
			ret = 1;
			goto out;
		}
		if (ctl.zbt_nr_zones != nr_zones) {
			fprintf(stderr, "Couldn't set FSNOZ %u, read back %u, max %u\n",
				nr_zones, ctl.zbt_nr_zones, ctl.zbt_max_activate);
			ret = 1;
			goto out;
		}
		nr_zones = 0;
	}

	if (opts->reset && reset_zones != 0) {
		/* Reset zones to avoid 4002 "Not Empty" error */
		ret = zbc_zone_group_op(dev, reset_start, reset_zones,
					ZBC_OP_RESET_ZONE,
					opts->all && !opts->skip_same ? ZBC_OP_ALL_ZONES : 0);
		if (ret != 0) {
			fprintf(stderr, "zone reset [#%"PRIu64":+%u] failed, err %i (%s)\n",
				reset_start, reset_zones, ret, strerror(-ret));
			ret = 1;
		}
	}

	ret = zbc_get_nr_actv_records(dev, !opts->fsnoz, opts->all, opts->cdb32, start_zone,
				      nr_zones, domain_id);
	if (ret < 0) {
		zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
		err_za = zbc_err.err_za;
		err_cbf = zbc_sect2lba(info, zbc_err.err_cbf);
		fprintf(stderr,
			"[TEST][ERROR],Can't get the number of activation records, err %i (%s)\n",
			ret, strerror(-ret));
		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
		if (err_za || err_cbf) {
			printf("[TEST][ERROR][ERR_ZA],0x%04x\n", err_za);
			printf("[TEST][ERROR][ERR_CBF],%"PRIu64"\n", err_cbf);
		}
		ret = 1;
		goto out;
	}
	nr_actv_recs = (uint32_t)ret;

	/* Allocate activation results record array */
	actv_recs = (struct zbc_actv_res *)calloc(nr_actv_recs,
						  sizeof(struct zbc_actv_res));
	if (!actv_recs) {
		fprintf(stderr, "[TEST][ERROR],No memory\n");
		goto out;
	}

	/* Activate zones */
	if (opts->query)
		ret = zbc_zone_query(dev, !opts->fsnoz, opts->all,
				     opts->cdb32, start_zone, nr_zones,
				     domain_id, actv_recs, &nr_actv_recs);
	else
		ret = zbc_zone_activate(dev, !opts->fsnoz, opts->all,
					opts->cdb32, start_zone, nr_zones,
					domain_id, actv_recs, &nr_actv_recs);

	if (ret != 0) {
		zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
		err_za = zbc_err.err_za;
		err_cbf = zbc_sect2lba(info, zbc_err.err_cbf);
		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
		if (err_za || err_cbf) {
			printf("[TEST][ERROR][ERR_ZA],0x%04x\n", err_za);
			printf("[TEST][ERROR][ERR_CBF],%"PRIu64"\n", err_cbf);
		}
	}
	for (i = 0; i < (int)nr_actv_recs; i++) {
		cr = &actv_recs[i];
		printf("[ACTV_RECORD],%"PRIu64",%"PRIu64",%x,%x,%x\n",
		       cr->zbe_start_zone,
		       cr->zbe_nr_zones,
		       cr->zbe_domain,
		       cr->zbe_type,
		       cr->zbe_condition);
	}

out:
	if (actv_recs)
		free(actv_recs);
	return ret;
}
static int perform_activation(struct zbc_device *dev, struct zbc_device_info *info,
                              struct cmd_options *opts)
{
	struct zbc_zone_realm *realms = NULL, *r;
	struct zbc_realm_item *ri;
	const char *sk_name, *ascq_name;
	uint64_t start, chunk_start, reset_start = (uint64_t)-1;
	struct zbc_err_ext zbc_err;
	unsigned int nr_units, domain_id = 0xffffffff;
	unsigned int nr_realms, chunk_zones, reset_zones = 0;
	int ret = 0, i, end;

	start = opts->start;
	nr_units = opts->nr_units;

	if (!opts->zone_addr) {
		/*
		 * Have to call zbc_list_zone_realms() to find the
		 * starting zone and number of zones to activate.
		 */
		ret = zbc_list_zone_realms(dev, 0LL, ZBC_RR_RO_ALL, &realms, &nr_realms);
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

		end = start + nr_units;
		if (end > (int)nr_realms) {
			fprintf(stderr,
				"[TEST][ERROR],Realm [%"PRIu64"/%u] out of range\n",
				start, nr_units);
			ret = 2;
			goto out;
		}

		/* Find domain ID for the new zone type */
		r = &realms[start];
		ri = zbc_realm_item_by_type(r, opts->new_type);
		if (ri == NULL) {
			fprintf(stderr,
				"[TEST][ERROR],Realm %"PRIu64" doesn't support zone type %u (%s)\n",
				start, opts->new_type, zbc_zone_type_str(opts->new_type));
			ret = 2;
			goto out;
		}
		domain_id = ri->zbi_dom_id;
		if (domain_id != opts->domain_id) {
			fprintf(stderr,
				"[TEST][ERROR],Inconsistent domain ID %u in realm #%"PRIu64", expecting %u\n",
				domain_id, start, opts->domain_id);
			ret = 1;
			goto out;
		}

		/* Set the start LBA and the length in zones */
		for (nr_units = 0, i = start; i < end; i++) {
			nr_units += zbc_realm_length(&realms[i], domain_id);
			if (opts->reset && !opts->skip_same)
				reset_zones += zbc_realm_length(&realms[i],
								r->zbr_dom_id);
		}
		if (!nr_units) {
			fprintf(stderr,
				"[TEST][ERROR],Realm #%"PRIu64" (start LBA %"PRIu64") has no zones to activate in domain %u\n",
				start, zbc_realm_start_lba(dev, r, domain_id), domain_id);
			ret = 1;
			goto out;
		}

		start = zbc_lba2sect(info,
				     zbc_realm_start_lba(dev, r, domain_id));
		if (opts->reset && !opts->skip_same)
			reset_start =
				zbc_lba2sect(info,
					     zbc_realm_start_lba(dev, r,
							r->zbr_dom_id));
	} else {
		start = zbc_lba2sect(info, start);
		domain_id = opts->domain_id;
		end = 0;
	}

	/* Decide if we should set the number of zones via FSNOZ */
	if (opts->all)
		opts->fsnoz = false;
	else if (!(info->zbd_flags & ZBC_NOZSRC_SUPPORT))
		opts->fsnoz = true;
	else if (opts->cdb32)
		;
	else if (nr_units > 0xffff)
		opts->fsnoz = true;

	if (!opts->skip_same) {
		ret = chunk_activation(dev, info, opts, start, nr_units, domain_id,
				       reset_start, reset_zones);
	} else {
		start = opts->start;
		for (chunk_zones = 0, i = start; i < end; i++) {
			r = &realms[i];
			if (zbc_realm_activation_allowed(r) &&
			    zbc_realm_zone_type(r, r->zbr_dom_id) != opts->new_type) {
				if (chunk_zones == 0) {
					chunk_start = zbc_lba2sect(info,
						zbc_realm_start_lba(dev, r, domain_id));
					if (opts->reset)
						reset_start =
							zbc_lba2sect(info,
								     zbc_realm_start_lba(dev,
								     r, r->zbr_dom_id));
				}
				chunk_zones += zbc_realm_length(r, domain_id);
				if (opts->reset)
					reset_zones += zbc_realm_length(r, r->zbr_dom_id);
			} else if (chunk_zones != 0) {
				ret = chunk_activation(dev, info, opts, chunk_start, chunk_zones,
						       domain_id, reset_start, reset_zones);
				chunk_zones = 0;
				reset_zones = 0;
				if (ret != 0)
					break;
			} else if (opts->reset && reset_zones == 0) {
				reset_start = zbc_lba2sect(info,
					zbc_realm_start_lba(dev,
							    r, r->zbr_dom_id));
				reset_zones += zbc_realm_length(r, r->zbr_dom_id);
			}
		}
		if (chunk_zones != 0) {
			ret = chunk_activation(dev, info, opts, chunk_start, chunk_zones,
					       domain_id, reset_start, reset_zones);
		}
	}
out:
	if (realms)
		free(realms);

	return ret;
}


int main(int argc, char **argv)
{
	struct zbc_device *dev;
	char *path;
	struct zbc_zone_domain *domains = NULL, *d;
	struct zbc_device_info info;
	struct cmd_options opts;
	unsigned int oflags, nr_domains;
	int i, ret, end_realm;

	/* Check command line */
	if (argc < 5) {
		printf("Usage: %s [options] <dev> <start zone realm> <num realms> <conv|seq|sobr|seqp>\n"
		       "or\n%s -z [options] <dev> <start zone LBA> <num zones> <conv|seq|sobr|seqp>\n"
		       "Options:\n"
		       "    -v              : Verbose mode\n"
		       "    -q | --query    : Query only, do not activate\n"
		       "    -a              : Try to activate all, even if not every zone can be\n"
		       "    -n | --fsnoz    : Set the number of zones to activate via FSNOZ\n"
		       "    -r              : Reset zones before activation (ignored for query and zone addressing)\n"
		       "    -s | -skip_same : Skip realms active to the same zone type instead of letting\n"
		       "                      the activation fail (ignored for query and zone addressing)\n"
		       "    -32             : Force using 32-byte SCSI command (16 by default)\n",
		       argv[0], argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-q") == 0) {
			opts.query = true;
		} else if (strcmp(argv[i], "--query") == 0) {
			opts.query = true;
		} else if (strcmp(argv[i], "-a") == 0) {
			opts.all = true;
		} else if (strcmp(argv[i], "-z") == 0) {
			opts.zone_addr = true;
		} else if (strcmp(argv[i], "-32") == 0) {
			opts.cdb32 = true;
		} else if (strcmp(argv[i], "-n") == 0) {
			opts.fsnoz = true;
		} else if (strcmp(argv[i], "--fsnoz") == 0) {
			opts.fsnoz = true;
		} else if (strcmp(argv[i], "-r") == 0) {
			opts.reset = true;
		} else if (strcmp(argv[i], "-skip_same") == 0) {
			opts.skip_same = true;
		} else if (strcmp(argv[i], "-s") == 0) {
			opts.skip_same = true;
		} else {
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

	if (opts.all) {
		/*
		 * FIXME make zone ID and size to follow the new zone type.
		 * This way, just omitting these for all would be possible.
		*/
		i += 2;
		opts.zone_addr = true;
	} else {
		if (i >= argc) {
			fprintf(stderr, "[TEST][ERROR],Missing starting %s\n",
				opts.zone_addr ? "zone LBA" : "zone realm");
			return 1;
		}
		opts.start = atol(argv[i++]);

		if (i >= argc || !isdigit(*argv[i])) {
			fprintf(stderr,
				"[TEST][ERROR],Missing number of %ss to activate\n",
				opts.zone_addr ? "zone" : "zone realm");
		return 1;
		}
		opts.nr_units = atoi(argv[i++]);
	}
	if (opts.zone_addr) {
		opts.skip_same = false;
	}

	if (i >= argc) {
		fprintf(stderr, "[TEST][ERROR],Missing new zone type\n");
		return 1;
	}
	if (strcmp(argv[i], "conv") == 0) {
		opts.new_type = ZBC_ZT_CONVENTIONAL;
	} else if (strcmp(argv[i], "sobr") == 0) {
		opts.new_type = ZBC_ZT_SEQ_OR_BEF_REQ;
	} else if (strcmp(argv[i], "seq") == 0) {
		opts.new_type = ZBC_ZT_SEQUENTIAL_REQ;
	} else if (strcmp(argv[i], "seqp") == 0) {
		opts.new_type = ZBC_ZT_SEQUENTIAL_PREF;
	} else {
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

	if (!zbc_device_is_zdr(&info)) {
		fprintf(stderr,
			"[TEST][ERROR],not a ZDR device\n");
		ret = 1;
		goto close;
	}

	/* Find domain ID for the new zone type */
	ret = zbc_list_domains(dev, 0LL, ZBC_RZD_RO_ALL,
			       &domains, &nr_domains);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_list_domains failed, err %i (%s) %s\n",
			ret, strerror(-ret), path);
		ret = 1;
		goto close;
	}

	for (i = 0, d = domains; i < (int)nr_domains; i++, d++) {
		if (d->zbm_type == opts.new_type) {
			opts.domain_id = i;
			break;
		}
	}
	if (i >= (int)nr_domains) {
		fprintf(stderr,
			"[TEST][ERROR],Device doesn't support zone type %u\n (%s)",
			opts.new_type, zbc_zone_type_str(opts.new_type));
		ret = 2;
		goto free_dom;
	}
	if (!(d->zbm_flags & ZBC_ZDF_VALID_ZONE_TYPE)) {
		fprintf(stderr,
			"[TEST][ERROR],Target zone domain %u has invalid zone type",
			opts.domain_id);
		ret = 1;
		goto free_dom;
	}

	if ((d->zbm_flags & ZBC_ZDF_SHIFTING_BOUNDARIES) && !opts.zone_addr) {
		end_realm = opts.start + opts.nr_units;
		opts.nr_units = 1;
		for (i = opts.start; i < end_realm; i++) {
			ret = perform_activation(dev, &info, &opts);
			if (ret)
				break;
			opts.start++;
		}
	} else {
		ret = perform_activation(dev, &info, &opts);
	}

free_dom:
	free(domains);
close:
	zbc_close(dev);

	return ret;
}

