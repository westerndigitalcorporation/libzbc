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
	struct zbc_cvt_domain *domains = NULL;
	struct zbc_conv_rec *conv_recs = NULL;
	char *path;
	struct zbc_zp_dev_control ctl;
	uint64_t start;
	unsigned int nr_units, nr_domains, nr_conv_recs = 0, new_type;
	int i, ret = 1, end;
	bool query = false, fsnoz = false;
	bool all = false, zone_addr = false, list = false, cdb32 = false;

	/* Check command line */
	if (argc < 5) {
		fprintf(stderr, "Not enough arguments\n");
usage:
		printf("Usage:\n%s [options] <dev> <start domain> <num domains> <conv | seq>\n"
		       "or\n%s -z [options] <dev> <start zone lba> <num zones> <conv | seq>\n"
		       "Options:\n"
		       "    -v            : Verbose mode\n"
		       "    -q            : Query only\n"
		       "    -a            : Convert all\n"
		       "    -n            : Set the number of zones to convert via separate call\n"
		       "    -32           : Use 32-byte SCSI commands, default is 16\n"
		       "    -l            : List conversion records\n",
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
			zone_addr ? "zone" : "conversion domain");
		goto usage;
	}
	start = atol(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "Missing the number of %ss to convert\n",
			zone_addr ? "zone" : "conversion domain");
		goto usage;
	}
	nr_units = atoi(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "Missing new zone type\n");
		goto usage;
	}
	if (strcmp(argv[i], "conv") == 0)
		new_type = ZBC_ZT_CONVENTIONAL;
	else if (strcmp(argv[i], "seq") == 0)
		new_type = ZBC_ZT_SEQUENTIAL_REQ;
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
		 * Have to call zbc_list_conv_domains() to find the
		 * starting zone and number of zones to convert.
		 */
		ret = zbc_list_conv_domains(dev, &domains, &nr_domains);
		if (ret != 0) {
			fprintf(stderr,
				"zbc_list_conv_domains failed, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
			goto out;
		}

		if (start + nr_units > nr_domains) {
			fprintf(stderr,
				"End end domain #%lu is too large, only %u present\n",
				start + nr_units, nr_domains);
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

	ret = zbc_get_nr_cvt_records(dev, all, cdb32, start,
				     nr_units, new_type);
	if (ret < 0) {
		fprintf(stderr,
			"Can't receive the number of conversion records, err %i (%s)\n",
			ret, strerror(-ret));
		ret = 1;
		goto out;
	}
	nr_conv_recs = ret;

	/* Allocate conversion record array */
	conv_recs = (struct zbc_conv_rec *)calloc(nr_conv_recs,
						  sizeof(struct zbc_conv_rec));
	if (!conv_recs) {
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
		/* Set the number of zones to convert via a separate command */
		ctl.zbm_nr_zones = nr_units;
		ctl.zbm_smr_zone_type = 0xff;
		ctl.zbm_cmr_wp_check = 0xff;
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
		ret = zbc_zone_query(dev, all, cdb32, start, nr_units,
				     new_type, conv_recs, &nr_conv_recs);
	else if (list)
		ret = zbc_zone_activate(dev, all, cdb32, start, nr_units,
					new_type, conv_recs, &nr_conv_recs);
	else
		ret = zbc_zone_activate(dev, all, cdb32, start, nr_units,
					new_type, NULL, &nr_conv_recs);

	if (ret != 0) {
		fprintf(stderr,
			"ZONE ACTIVATE/QUERY failed, err %i (%s)\n",
			ret, strerror(-ret));
		ret = 1;
		goto out;
	}

	if (list) {
		for (i = 0; i < (int)nr_conv_recs; i++) {
			printf("%03i LBA:%012lu Size:%08u Type:%02Xh Cond:%02Xh\n",
			       i, conv_recs[i].zbe_start_zone,
			       conv_recs[i].zbe_nr_zones,
			       conv_recs[i].zbe_type,
			       conv_recs[i].zbe_condition);
		}
	}
out:
	if (domains)
		free(domains);
	if (conv_recs)
		free(conv_recs);
	zbc_close(dev);

	return ret;
}

