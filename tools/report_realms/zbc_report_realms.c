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

static void zbc_print_realm(struct zbc_device_info *info,
			    struct zbc_zone_realm *r)
{
	if (zbc_zone_realm_conventional(r) || zbc_zone_realm_wpc(r) ||
	    zbc_zone_realm_sequential(r) ||zbc_zone_realm_seq_pref(r)) {
		printf("%03d: type 0x%x (%s), conv LBA %08llu:"
		       "%u zones, seq LBA %08llu:%u zones, kpo %u, "
		       "cvt to conv: %s, cvt to seq: %s\n",
		       zbc_zone_realm_number(r), zbc_zone_realm_type(r),
		       zbc_zone_type_str(zbc_zone_realm_type(r)),
		       zbc_zone_realm_conv_start(r),
		       zbc_zone_realm_conv_length(r),
		       zbc_zone_realm_seq_start(r),
		       zbc_zone_realm_seq_length(r),
		       zbc_zone_realm_keep_out(r),
		       zbc_zone_realm_to_conv(r) ? "Y" : "N",
		       zbc_zone_realm_to_seq(r) ? "Y" : "N");
		return;
	}

	printf("Zone realm %03d: unknown type 0x%x\n",
	       zbc_zone_realm_number(r), zbc_zone_realm_type(r));
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int nr_realms = 0, nd = 0;
	struct zbc_zone_realm *realms = NULL;
	int i, ret = 1, num = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -n		  : Get only the number of realm descriptors\n"
		       "  -nd <num>	  : Get at most <num> realm descriptors\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0)
			zbc_set_log_level("debug");
		else if (strcmp(argv[i], "-n") == 0)
			num = 1;
		else if (strcmp(argv[i], "-nd") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			nd = strtol(argv[i], NULL, 10);
			if (nd <= 0)
				goto usage;
		} else {
			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			goto usage;
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

	ret = zbc_report_nr_realms(dev, &nr_realms);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_nr_realms failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	printf("    %u zone realm%s\n",
	       nr_realms, (nr_realms > 1) ? "s" : "");
	if (num)
		goto out;

	if (!nd || nd > nr_realms)
		nd = nr_realms;
	if (!nd)
		goto out;

	/* Allocate zone realm descriptor array */
	realms = (struct zbc_zone_realm *)calloc(nd,
						 sizeof(struct zbc_zone_realm));
	if (!realms) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	/* Get the realm descriptors */
	ret = zbc_report_realms(dev, realms, &nd);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_realms failed %d\n", ret);
		ret = 1;
		goto out;
	}

	for (i = 0; i < (int)nd; i++)
		zbc_print_realm(&info, &realms[i]);

out:
	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

