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
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

#define ZBC_O_DRV_MASK (ZBC_O_DRV_BLOCK | ZBC_O_DRV_SCSI | ZBC_O_DRV_ATA)

static void zbc_report_print_range(struct zbc_device_info *info,
				   struct zbc_cvt_range *r)
{
	if (zbc_cvt_range_conventional(r) || zbc_cvt_range_sequential(r)) {
		printf("%03d: type 0x%x (%s), conv LBA %08llu:"
		       "%u zones, seq LBA %08llu:%u zones, kpo %u, "
		       "cvt to conv: %s, cvt to seq: %s\n",
		       zbc_cvt_range_number(r), zbc_cvt_range_type(r),
		       zbc_zone_type_str(zbc_cvt_range_type(r)),
		       zbc_cvt_range_conv_start(r),
		       zbc_cvt_range_conv_length(r),
		       zbc_cvt_range_seq_start(r),
		       zbc_cvt_range_seq_length(r),
		       zbc_cvt_range_keep_out(r),
		       zbc_cvt_range_to_conv(r) ? "Y" : "N",
		       zbc_cvt_range_to_seq(r) ? "Y" : "N");
		return;
	}

	printf("Conversion range %03d: unknown type 0x%x\n",
	       zbc_cvt_range_number(r), zbc_cvt_range_type(r));
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int nr_ranges = 0, nr = 0;
	struct zbc_cvt_range *ranges = NULL;
	int i, ret = 1, num = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -n		  : Get only the number of range descriptors\n"
		       "  -nr <num>	  : Get at most <num> range descriptors\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-n") == 0) {
			num = 1;
		} else if (strcmp(argv[i], "-nr") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			nr = strtol(argv[i], NULL, 10);
			if (nr <= 0)
				goto usage;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			goto usage;
		} else {
			break;
		}

	}

	if (i != (argc - 1))
		goto usage;
	path = argv[i];

	/* Open device */
	ret = zbc_open(path, ZBC_O_DRV_MASK | O_RDONLY, &dev);
	if (ret != 0)
		return 1;

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	ret = zbc_media_report_nr_ranges(dev, &nr_ranges);
	if (ret != 0) {
		fprintf(stderr, "zbc_media_report_nr_ranges failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	printf("    %u conversion ranges%s\n", nr_ranges, (nr_ranges > 1) ? "s" : "");
	if (num)
		goto out;

	if (!nr || nr > nr_ranges)
		nr = nr_ranges;
	if (!nr)
		goto out;

	/* Allocate conversion range descriptor array */
	ranges = (struct zbc_cvt_range *)calloc(nr,
						sizeof(struct zbc_cvt_range));
	if (!ranges) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	/* Get the range descriptors */
	ret = zbc_media_report(dev, ranges, &nr);
	if (ret != 0) {
		fprintf(stderr, "zbc_media_report failed %d\n", ret);
		ret = 1;
		goto out;
	}

	for (i = 0; i < (int)nr; i++)
		zbc_report_print_range(&info, &ranges[i]);

out:
	if (ranges)
		free(ranges);
	zbc_close(dev);

	return ret;
}

