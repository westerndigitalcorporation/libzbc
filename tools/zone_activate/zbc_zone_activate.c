/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2018, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public License version 3, "as is," without technical
 * support, and WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
 * received a copy of the GNU Lesser General Public License along with libzbc.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Dmitry Fomichev (dmitry.fomichev@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

#define ZBC_O_DRV_MASK (ZBC_O_DRV_BLOCK | ZBC_O_DRV_SCSI | \
			ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE)

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	struct zbc_zone_domain *domains = NULL, *d;
	struct zbc_zone_realm *realms = NULL, *r;
	struct zbc_realm_item *ri;
	struct zbc_actv_res *actv_recs = NULL;
	char *path;
	struct zbc_zp_dev_control ctl;
	uint64_t start;
	unsigned int nr_units, nr_realms, nr_actv_recs = 0, new_type;
	unsigned int nr_domains, domain_id;
	int i, ret = 1, end;
	bool query = false, fsnoz = false;
	bool all = false, zone_addr = false, list = false, cdb32 = false;

	/* Check command line */
	if (argc < 5) {
		fprintf(stderr, "Not enough arguments\n");
usage:
		printf("Usage:\n%s [options] <dev> <start realm> <num realms> <conv|seq[r]|sobr|seqp>\n"
		       "or\n%s -z [options] <dev> <start zone lba> <num zones> <conv|seq[r]|sobr|seqp>\n"
		       "Options:\n"
		       "    -v            : Verbose mode\n"
		       "    -q            : Query only\n"
		       "    -a            : Activate all\n"
		       "    -n            : Set the number of zones to activate via a separate call\n"
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

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0)
			zbc_set_log_level("debug");
		else if (strcmp(argv[i], "-q") == 0) {
			query = true;
			list = true;
		} else if (strcmp(argv[i], "-a") == 0)
			all = true;
		else if (strcmp(argv[i], "-n") == 0)
			fsnoz = true;
		else if (strcmp(argv[i], "-32") == 0)
			cdb32 = true;
		else if (strcmp(argv[i], "-l") == 0)
			list = true;
		else if (strcmp(argv[i], "-z") == 0) {
			zone_addr = true;
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

	if (i >= argc) {
		fprintf(stderr, "Missing starting %s\n",
			zone_addr ? "zone" : "zone realm");
		goto usage;
	}
	start = atol(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "Missing the number of %ss to activate\n",
			zone_addr ? "zone" : "realm");
		goto usage;
	}
	nr_units = atoi(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "Missing new zone type\n");
		goto usage;
	}
	if (strcmp(argv[i], "conv") == 0)
		new_type = ZBC_ZT_CONVENTIONAL;
	else if (strcmp(argv[i], "sobr") == 0)
		new_type = ZBC_ZT_SEQ_OR_BEF_REQ;
	else if (strcmp(argv[i], "seq") == 0 ||	strcmp(argv[i], "seqr") == 0)
		new_type = ZBC_ZT_SEQUENTIAL_REQ;
	else if (strcmp(argv[i], "seqp") == 0)
		new_type = ZBC_ZT_SEQUENTIAL_PREF;
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
	ret = zbc_open(path, ZBC_O_DRV_MASK | O_RDWR, &dev);
	if (ret != 0) {
		fprintf(stderr,
			"zbc_open failed, err %i (%s)\n",
			ret, strerror(-ret));
		return 1;
	}

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	if (!zone_addr) {
		/*
		 * Have to call zbc_list_zone_realms() to find the
		 * starting zone and the number of zones to activate.
		 */
		ret = zbc_list_zone_realms(dev, &realms, &nr_realms);
		if (ret != 0) {
			fprintf(stderr,
				"zbc_list_zone_realms failed, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
			goto out;
		}

		if (start + nr_units > nr_realms) {
			fprintf(stderr,
				"End realm #%lu is too large, only %u present\n",
				start + nr_units, nr_realms);
			ret = 1;
			goto out;
		}
		end = start + nr_units;

		/* Find domain ID for the new zone type */
		r = &realms[start];
		ri = zbc_realm_item_by_type(r, new_type);
		if (ri == NULL) {
			fprintf(stderr,
				"Start realm #%lu doesn't support zone type %u\n",
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
			fprintf(stderr, "zbc_list_domains failed %d\n", ret);
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
				"Device doesn't support zone type %u\n",
				new_type);
			ret = 1;
			goto out;
		}
	}

	ret = zbc_get_nr_actv_records(dev, !fsnoz, all, cdb32, start,
				      nr_units, domain_id);
	if (ret < 0) {
		fprintf(stderr,
			"Can't receive the number of activation records, err %i (%s)\n",
			ret, strerror(-ret));
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

	/*
	 * Force setting the number of zones via FSNOZ
	 * if it doesn't fit into 16-bit word.
	 */
	if (!cdb32 && nr_units > 0xffff)
		fsnoz = true;

	if (fsnoz) {
		/* Set the number of zones to activate via a separate command */
		ctl.zbm_nr_zones = nr_units;
		ctl.zbm_urswrz = 0xff;
		ctl.zbm_max_activate = 0xffff;
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			fprintf(stderr, "Can't set FSNOZ, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
			goto out;
		}
		nr_units = 0;
	}

	if (query)
		ret = zbc_zone_query(dev, !fsnoz, all, cdb32, start, nr_units,
				     domain_id, actv_recs, &nr_actv_recs);
	else if (list)
		ret = zbc_zone_activate(dev, !fsnoz, all, cdb32, start, nr_units,
					domain_id, actv_recs, &nr_actv_recs);
	else
		ret = zbc_zone_activate(dev, !fsnoz, all, cdb32, start, nr_units,
					domain_id, NULL, &nr_actv_recs);

	if (ret != 0) {
		fprintf(stderr,
			"ZONE ACTIVATE/QUERY failed, err %i (%s)\n",
			ret, strerror(-ret));
		ret = 1;
		goto out;
	}

	if (list) {
		for (i = 0; i < (int)nr_actv_recs; i++) {
			printf("%03i LBA:%012lu Size:%08u Type:%02Xh Cond:%02Xh\n",
			       i, actv_recs[i].zbe_start_zone,
			       actv_recs[i].zbe_nr_zones,
			       actv_recs[i].zbe_type,
			       actv_recs[i].zbe_condition);
		}
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

