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
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

/* Parsed command line options */
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
	bool list;
	bool cdb32;
	bool reset;
};

static int perform_activation(struct zbc_device *dev, struct zbc_device_info *info,
			      struct cmd_options *opts)
{
	struct zbc_zone_realm *realms = NULL, *r;
	struct zbc_actv_res *actv_recs = NULL;
	const char *sk_name, *ascq_name;
	struct zbc_realm_item *ri;
	uint64_t start;
	struct zbc_err_ext zbc_err;
	struct zbc_zd_dev_control ctl;
	unsigned int nr_units, domain_id = 0xffffffff;
	unsigned int nr_realms, nr_actv_recs, reset_zones = 0;
	uint64_t err_cbf, reset_start = (uint64_t)-1;
	uint16_t err_za;
	int ret = 0, i, end;

	start = opts->start;
	nr_units = opts->nr_units;

	if (!opts->zone_addr) {
		/*
		 * Have to call zbc_list_zone_realms() to find the
		 * starting zone and the number of zones to activate.
		 */
		ret = zbc_list_zone_realms(dev, 0LL, ZBC_RR_RO_ALL, &realms, &nr_realms);
		if (ret != 0) {
			fprintf(stderr,
				"zbc_list_zone_realms failed, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
			goto out;
		}

		end = start + nr_units;
		if (end > (int)nr_realms) {
			fprintf(stderr,
				"End realm %d is too large, only %u present\n",
				end, nr_realms);
			ret = 1;
			goto out;
		}

		/* Find domain ID for the new zone type */
		r = &realms[start];
		ri = zbc_realm_item_by_type(r, opts->new_type);
		if (ri == NULL) {
			fprintf(stderr,
				"Start realm #%"PRIu64" doesn't support zone type %u\n",
				start, opts->new_type);
			ret = 1;
			goto out;
		}
		domain_id = ri->zbi_dom_id;
		if (domain_id != opts->domain_id) {
			fprintf(stderr,
				"Inconsistent domain ID %u in realm #%"PRIu64", expecting %u\n",
				domain_id, start, opts->domain_id);
			ret = 1;
			goto out;
		}

		/* Set the start LBA and the length in zones */
		for (nr_units = 0, i = start; i < end; i++) {
			nr_units += zbc_realm_length(&realms[i], domain_id);
			if (opts->reset)
				reset_zones += zbc_realm_length(&realms[i],
								r->zbr_dom_id);
		}
		if (!nr_units) {
			fprintf(stderr,
				"Realm #%"PRIu64" (start LBA %"PRIu64") has no zones to activate in domain %u\n",
				start, zbc_realm_start_lba(dev, r, domain_id),
				domain_id);
			ret = 1;
			goto out;
		}

		start = zbc_lba2sect(info,
				     zbc_realm_start_lba(dev, r, domain_id));
		if (opts->reset)
			reset_start =
				zbc_lba2sect(info,
					     zbc_realm_start_lba(dev, r,
							 r->zbr_dom_id));
	} else {
		if (opts->lba_units)
			start = zbc_lba2sect(info, start);
		domain_id = opts->domain_id;
	}

	/* Decide if we should set the number of zones via FSNOZ */
	if (opts->all) {
		opts->fsnoz = false;
	} else if (!(info->zbd_flags & ZBC_NOZSRC_SUPPORT)) {
		if (!opts->fsnoz && opts->verbose)
			fprintf(stderr,
				"Device doesn't support NOZSRC, forcing -n flag\n");
		opts->fsnoz = true;
	} else if (opts->cdb32) {
		;
	} else if (nr_units > 0xffff)
		opts->fsnoz = true;

	if (opts->fsnoz) {
		/* Make sure the device supports this */
		if (!(info->zbd_flags & ZBC_ZA_CONTROL_SUPPORT)) {
			fprintf(stderr, "Device doesn't support setting FSNOZ\n");
			ret = 1;
			goto out;
		}

		/* Set the number of zones to activate via a separate command */
		ctl.zbt_nr_zones = nr_units;
		ctl.zbt_urswrz = 0xff;
		ctl.zbt_max_activate = 0xffff;
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			fprintf(stderr, "Can't set FSNOZ, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
			goto out;
		}
	}

	if (opts->reset) {
		/* Reset zones to avoid 4002 "Not Empty" error */
		ret = zbc_zone_group_op(dev, reset_start, reset_zones,
					ZBC_OP_RESET_ZONE,
					opts->all ? ZBC_OP_ALL_ZONES : 0);
		if (ret != 0) {
			fprintf(stderr, "zone reset [#%"PRIu64":+%u] failed, err %i (%s)\n",
				reset_start, reset_zones, ret, strerror(-ret));
			ret = 1;
		}
	}

	if (opts->fsnoz)
		nr_units = 0;

	ret = zbc_get_nr_actv_records(dev, !opts->fsnoz, opts->all, opts->cdb32, start,
				      nr_units, domain_id);
	if (ret < 0) {
		fprintf(stderr,
			"Can't receive the number of activation records, err %i (%s)\n",
			ret, strerror(-ret));
		if (opts->verbose) {
			zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			err_za = zbc_err.err_za;
			err_cbf = opts->lba_units ?
				  zbc_sect2lba(info, zbc_err.err_cbf) :
				  zbc_err.err_cbf;
			printf("SK='%s', ASC_ASCQ='%s'\n", sk_name, ascq_name);
			if (err_za || err_cbf)
				printf("ERR_ZA=0x%04x, ERR_CBF=%"PRIu64"\n",
				       err_za, err_cbf);
		}
		ret = 1;
		goto out;
	}
	nr_actv_recs = ret;

	/* Allocate activation results record array */
	actv_recs = (struct zbc_actv_res *)calloc(nr_actv_recs,
						  sizeof(struct zbc_actv_res));
	if (!actv_recs) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	if (opts->query)
		ret = zbc_zone_query(dev, !opts->fsnoz, opts->all, opts->cdb32, start, nr_units,
				     domain_id, actv_recs, &nr_actv_recs);
	else if (opts->list)
		ret = zbc_zone_activate(dev, !opts->fsnoz, opts->all, opts->cdb32, start, nr_units,
					domain_id, actv_recs, &nr_actv_recs);
	else
		ret = zbc_zone_activate(dev, !opts->fsnoz, opts->all, opts->cdb32, start, nr_units,
					domain_id, NULL, &nr_actv_recs);

	if (ret != 0) {
		fprintf(stderr,
			"ZONE ACTIVATE/QUERY failed, err %i (%s)\n",
			ret, strerror(-ret));
		if (opts->verbose) {
			zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
			sk_name = zbc_sk_str(zbc_err.sk);
			ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
			err_za = zbc_err.err_za;
			err_cbf = opts->lba_units ?
				  zbc_sect2lba(info, zbc_err.err_cbf) :
				  zbc_err.err_cbf;
			printf("SK='%s', ASC_ASCQ='%s'\n", sk_name, ascq_name);
			if (err_za || err_cbf)
				printf("ERR_ZA=0x%04x, ERR_CBF=%"PRIu64"\n",
				       err_za, err_cbf);
		}
		ret = 1;
		goto out;
	}

	if (opts->list) {
		for (i = 0; i < (int)nr_actv_recs; i++) {
			printf("%03i LBA:%012"PRIu64" Size:%08"PRIu64" Dom:%02Xh Type:%02Xh Cond:%02Xh\n",
			       i,
			       opts->lba_units ?
					actv_recs[i].zbe_start_zone :
					zbc_lba2sect(info, actv_recs[i].zbe_start_zone),
			       actv_recs[i].zbe_nr_zones,
			       actv_recs[i].zbe_domain,
			       actv_recs[i].zbe_type,
			       actv_recs[i].zbe_condition);
		}
	}
out:
	if (realms)
		free(realms);
	if (actv_recs)
		free(actv_recs);

	return ret;
}

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_zone_domain *domains, *d;
	char *path;
	struct zbc_device_info info;
	struct cmd_options opts;
	unsigned int nr_domains;
	int i, ret, end_realm, oflags = 0;

	/* Check command line */
	if (argc < 5) {
		fprintf(stderr, "Not enough arguments\n");
usage:
		printf("Usage:\n%s [options] <dev> <start realm> <num realms> <conv|seq[r]|sobr|seqp>\n"
		       "or\n%s -z [options] <dev> <start zone> <num zones> <conv|seq[r]|sobr|seqp>\n"
		       "Options:\n"
		       "    -v            : Verbose mode\n"
		       "    -scsi         : Force the use of SCSI passthrough commands\n"
		       "    -ata          : Force the use of ATA passthrough commands\n"
		       "    -lba          : Start zone is in logical block units (512B sectors by default)\n"
		       "    -q | --query  : Query only, do not activate\n"
		       "    -a            : Activate all\n"
		       "    -r            : Reset zones before activation (ignored for query and zone addressing)\n"
		       "    -n | --fsnoz  : Set the number of zones to activate via a separate call\n"
		       "    -32           : Use 32-byte SCSI commands, default is 16\n"
		       "    -l            : List activation results records\n\n"
		       "Zone types:\n"
		       "    conv          : conventional\n"
		       "    sobr          : sequential or before required\n"
		       "    seq or seqr   : sequential write required\n"
		       "    seqp          : sequential write preferred\n",
		       argv[0], argv[0]);
		return 1;
	}

	memset(&opts, 0, sizeof(opts));

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
			opts.verbose = true;
		} else if (strcmp(argv[i], "-scsi") == 0) {
			oflags = ZBC_O_DRV_SCSI;
		} else if (strcmp(argv[i], "-ata") == 0) {
			oflags = ZBC_O_DRV_ATA;
		} else if (strcmp(argv[i], "-lba") == 0) {
			opts.lba_units = true;
		} else if (strcmp(argv[i], "-q") == 0) {
			opts.query = true;
			opts.list = true;
		} else if (strcmp(argv[i], "--query") == 0) {
			opts.query = true;
			opts.list = true;
		} else if (strcmp(argv[i], "-a") == 0) {
			opts.all = true;
		} else if (strcmp(argv[i], "-r") == 0) {
			opts.reset = true;
		} else if (strcmp(argv[i], "-n") == 0) {
			opts.fsnoz = true;
		} else if (strcmp(argv[i], "--fsnoz") == 0) {
			opts.fsnoz = true;
		} else if (strcmp(argv[i], "-32") == 0) {
			opts.cdb32 = true;
		} else if (strcmp(argv[i], "-l") == 0) {
			opts.list = true;
		} else if (strcmp(argv[i], "-z") == 0) {
			opts.zone_addr = true;
		} else {
			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
			goto usage;
		}
	}

	if (i >= argc) {
		fprintf(stderr, "Missing zoned device path\n");
		goto usage;
	}
	path = argv[i++];

	if (opts.reset && (opts.query || opts.zone_addr))
		opts.reset = false;

	if (opts.all) {
		/*
		 * FIXME make zone ID and size to follow the new zone type.
		 * This way, just omitting these for all would be possible.
		 */
		i += 2;
		opts.zone_addr = true;
	} else {
		if (i >= argc) {
			fprintf(stderr, "Missing starting %s\n",
				opts.zone_addr ? "zone" : "zone realm");
			goto usage;
		}
		opts.start = atol(argv[i++]);

		if (i >= argc) {
			fprintf(stderr, "Missing the number of %ss to activate\n",
				opts.zone_addr ? "zone" : "realm");
			goto usage;
		}
		opts.nr_units = atoi(argv[i++]);
	}

	if (i >= argc) {
		fprintf(stderr, "Missing new zone type\n");
		goto usage;
	}
	if (strcmp(argv[i], "conv") == 0)
		opts.new_type = ZBC_ZT_CONVENTIONAL;
	else if (strcmp(argv[i], "sobr") == 0)
		opts.new_type = ZBC_ZT_SEQ_OR_BEF_REQ;
	else if (strcmp(argv[i], "seq") == 0 ||	strcmp(argv[i], "seqr") == 0)
		opts.new_type = ZBC_ZT_SEQUENTIAL_REQ;
	else if (strcmp(argv[i], "seqp") == 0)
		opts.new_type = ZBC_ZT_SEQUENTIAL_PREF;
	else {
		fprintf(stderr, "Invalid new zone type\n");
		goto usage;
	}

	i++;
	if (i < argc) {
		fprintf(stderr, "Extra parameter '%s'\n", argv[i]);
		goto usage;
	}

	/* Open device */
	ret = zbc_open(path, oflags | O_RDWR, &dev);
	if (ret != 0) {
		if (ret == -ENODEV)
			fprintf(stderr,
				"Open %s failed (not a zoned block device)\n",
				path);
		else
			fprintf(stderr,
				"zbc_open failed, err %i (%s)\n",
				ret, strerror(-ret));
		return 1;
	}

	zbc_get_device_info(dev, &info);

	if (opts.verbose) {
		printf("Device %s:\n", path);
		zbc_print_device_info(&info, stdout);
	}

	/* Find domain ID for the new zone type */
	ret = zbc_list_domains(dev, 0LL, ZBC_RZD_RO_ALL,
			       &domains, &nr_domains);
	if (ret != 0) {
		fprintf(stderr, "zbc_list_domains failed %d\n", ret);
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
			"Device doesn't support zone type %u\n",
			opts.new_type);
		ret = 1;
		goto free_dom;
	}
	if (!(d->zbm_flags & ZBC_ZDF_VALID_ZONE_TYPE)) {
		fprintf(stderr,
			"Target zone domain %u has invalid zone type",
			opts.domain_id);
		ret = 1;
		goto free_dom;
	}

	if ((d->zbm_flags & ZBC_ZDF_SHIFTING_BOUNDARIES) && !opts.zone_addr) {
		if (opts.verbose) {
			printf("Zone domain %u has shifting boundaries, activating realms one by one",
			       opts.domain_id);
		}
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
