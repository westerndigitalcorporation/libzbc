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

static void zbc_report_print_region(struct zbc_device_info *info,
				    struct zbc_realm *r)
{
	if (zbc_realm_conventional(r) || zbc_realm_sequential(r)) {
		printf("%03d: type 0x%x (%s), conv LBA %08llu:"
		       "%u zones, seq LBA %08llu:%u zones, kpo %u, "
		       "cvt to conv: %s, cvt to seq: %s\n",
		       zbc_realm_number(r), zbc_realm_type(r),
		       zbc_zone_type_str(zbc_realm_type(r)),
		       zbc_realm_conv_start(r), zbc_realm_conv_length(r),
		       zbc_realm_seq_start(r), zbc_realm_seq_length(r),
		       zbc_realm_keep_out(r),
		       zbc_realm_to_conv(r) ? "Y" : "N",
		       zbc_realm_to_seq(r) ? "Y" : "N");
		return;
	}

	printf("Realm %03d: unknown type 0x%x\n",
	       zbc_realm_number(r), zbc_realm_type(r));
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int nr_realms = 0, nr = 0;
	struct zbc_realm *realms = NULL;
	int i, ret = 1, num = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -n		  : Get only the number of realms\n"
		       "  -nr <num>	  : Get at most <num> realms\n",
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

	ret = zbc_media_report_nr_regions(dev, &nr_realms);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_nr_realms failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	printf("    %u realm%s\n", nr_realms, (nr_realms > 1) ? "s" : "");
	if (num)
		goto out;

	if (!nr || nr > nr_realms)
		nr = nr_realms;
	if (!nr)
		goto out;

	/* Allocate realm array */
	realms = (struct zbc_realm *)calloc(nr, sizeof(struct zbc_realm));
	if (!realms) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	/* Get realm information */
	ret = zbc_media_report(dev, realms, &nr);
	if (ret != 0) {
		fprintf(stderr, "zbc_media_report failed %d\n", ret);
		ret = 1;
		goto out;
	}

	for (i = 0; i < (int)nr; i++)
		zbc_report_print_region(&info, &realms[i]);

out:
	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

