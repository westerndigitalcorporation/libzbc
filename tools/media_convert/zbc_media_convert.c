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
	struct zbc_cvt_range *ranges = NULL;
	struct zbc_conv_rec *conv_recs = NULL;
	uint64_t start;
	unsigned int nr_units, nr_ranges, nr_conv_recs = 0;
	int i, type, fg = 0, ret = 1, end;
	char *path;
	bool media_cvt = false, query = false;
	bool all = false, zone_addr = false, list = false, cdb32 = true;

	/* Check command line */
	if (argc < 5) {
		fprintf(stderr, "Not enough arguments\n");
usage:
		printf("Usage:\n%s [options] <dev> <start conv range> "
		       "<num conv reanges> <conv | seq> [<fg>]\n"
		       "or\n%s -z [options] <dev> <start zone lba> "
		       "<num zones> <conv | seq> [<fg>]\n"
		       "Options:\n"
		       "    -v            : Verbose mode\n"
		       "    -m            : Use MEDIA CONVERT instead of CONVERT REALMS\n"
		       "    -q            : Query only\n"
		       "    -a            : Convert all\n"
		       "    -16           : Use 16-byte SCSI commands, default is 32\n"
		       "    -l            : List conversion records\n",
		       argv[0], argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (strcmp(argv[i], "-v") == 0)
			zbc_set_log_level("debug");
		else if (strcmp(argv[i], "-m") == 0)
			media_cvt = true;
		else if (strcmp(argv[i], "-q") == 0) {
			query = true;
			list = true;
		}
		else if (strcmp(argv[i], "-a") == 0)
			all = true;
		else if (strcmp(argv[i], "-16") == 0)
			cdb32 = false;
		else if (strcmp(argv[i], "-l") == 0)
			list = true;
		else if (strcmp(argv[i], "-z") == 0) {
			media_cvt = true;
			zone_addr = true;
		}
		else if ( argv[i][0] == '-' ) {
			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
			goto usage;
		}
		else
			break;
	}

	if (i >= argc) {
		fprintf(stderr, "Missing zoned device path\n");
		goto usage;
	}
	path = argv[i++];

	if (i >= argc) {
		if (zone_addr)
			fprintf(stderr, "Missing starting zone locator\n");
		else
			fprintf(stderr, "Missing start conversion range number\n");
		goto usage;
	}
	start = atol(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "Missing the number of %ss to convert\n",
			zone_addr ? "zone" : "conversion range");
		goto usage;
	}
	nr_units = atoi(argv[i++]);

	if (i >= argc) {
		fprintf(stderr, "Missing new zone type\n");
		goto usage;
	}
	if (strcmp(argv[i], "conv") == 0) {
		if (!media_cvt)
			type = ZBC_ZT_CONVENTIONAL;
		else
			type = ZBC_CVT_TO_CMR;
	} else if (strcmp(argv[i], "seq") == 0) {
		if (!media_cvt)
			type = ZBC_ZT_SEQUENTIAL_REQ;
		else
			type = ZBC_CVT_TO_SMR;
	} else {
		fprintf(stderr, "Invalid new zone type\n");
		goto usage;
	}

	i++;
	if (i < argc)
		fg = atoi(argv[i]);

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

	if (!media_cvt) {
		/* Convert realms */
		ret = zbc_convert_realms(dev, start, nr_units, type, fg);
		if (ret != 0) {
			fprintf(stderr,
				"zbc_convert_realms failed, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
		}
		goto out;
	}

	if (!zone_addr) {
		/*
		 * Have to call zbc_list_conv_ranges() to find the
		 * starting zone and number of zones to convert.
		 */
		ret = zbc_list_conv_ranges(dev, &ranges, &nr_ranges);
		if (ret != 0) {
			fprintf(stderr,
				"zbc_list_conv_ranges failed, err %i (%s)\n",
				ret, strerror(-ret));
			ret = 1;
			goto out;
		}

		if (start + nr_units > nr_ranges - 1) {
			fprintf(stderr,
				"End range #%lu is too large, only %u present\n",
				start + nr_units, nr_ranges);
			ret = 1;
			goto out;
		}
		end = start + nr_units;
		if (type == ZBC_CVT_TO_SMR) {
			for (nr_units = 0, i = start; i < end; i++)
				nr_units += ranges[i].zbr_seq_length;
			start = ranges[start].zbr_seq_start;
		}
		else {
			for (nr_units = 0, i = start; i < end; i++)
				nr_units += ranges[i].zbr_conv_length;
			start = ranges[start].zbr_conv_start;
		}
	}

	ret = zbc_media_query(dev, all, cdb32, start, nr_units,
			      type, fg, NULL, &nr_conv_recs);
	if (ret != 0) {
		fprintf(stderr,
			"Can't receive the number of conversion records, err %i (%s)\n",
			ret, strerror(-ret));
		ret = 1;
		goto out;
	}

	/* Allocate conversion record array */
	conv_recs = (struct zbc_conv_rec *)calloc(nr_conv_recs,
						  sizeof(struct zbc_conv_rec));
	if (!ranges) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	if (query)
		ret = zbc_media_query(dev, all, cdb32, start, nr_units,
				      type, fg, conv_recs, &nr_conv_recs);
	else if (list)
		ret = zbc_media_convert(dev, all, cdb32, start, nr_units,
					type, fg, conv_recs, &nr_conv_recs);
	else
		ret = zbc_media_convert(dev, all, cdb32, start, nr_units,
					type, fg, NULL, &nr_conv_recs);

	if (ret != 0) {
		fprintf(stderr,
			"MEDIA CONVERT/QUERY failed, err %i (%s)\n",
			ret, strerror(-ret));
		ret = 1;
		goto out;
	}

	if (list) {
		for (i = 0; i < (int)nr_conv_recs; i++) {
			printf("%03i %012lu %08u 0x%x 0x%x\n",
			       i, conv_recs[i].zbe_start_lba,
			       conv_recs[i].zbe_nr_zones,
			       conv_recs[i].zbe_type,
			       conv_recs[i].zbe_condition);
		}
	}
out:
	if (ranges)
		free(ranges);
	if (conv_recs)
		free(conv_recs);
	zbc_close(dev);

	return ret;
}

