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

#define ZBC_O_DRV_MASK (ZBC_O_DRV_BLOCK | ZBC_O_DRV_SCSI | \
			ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE)

static void zbc_report_print_realm(struct zbc_device_info *info,
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
		       zbc_realm_conv_to_conventional(r) ? "Y" : "N",
		       zbc_realm_conv_to_sequential(r) ? "Y" : "N");
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
	struct zbc_realm *r, *realms = NULL;
	int i, ret = 1, num = 0, flags = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -f <hex mask>   : Use the specified backend mask"
		       " to open the device\n"
		       "  -n		  : Get only the number of realms\n"
		       "  -nr <num>	  : Get at most <num> realms\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (strcmp(argv[i], "-v") == 0)
			zbc_set_log_level("debug");
		else if (strcmp(argv[i], "-n") == 0)
			num = 1;
		else if (strcmp(argv[i], "-nr") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			nr = strtol(argv[i], NULL, 10);
			if (nr <= 0)
				goto usage;
		}
		else if (strcmp(argv[i], "-f") == 0) {
			if (i >= (argc - 1)) {
				fprintf(stderr, "Missing backend flag value\n");
				goto usage;
			}
			i++;

			flags = strtol(argv[i], NULL, 16);
			if (flags != 0) {
				if (flags < 0) {
					fprintf(stderr, "Invalid backend flag %i\n",
						flags);
					goto usage;
				}
				flags &= ZBC_O_DRV_MASK;
			}
		}
		else if (argv[i][0] == '-') {
			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			goto usage;
		}
		else
			break;
	}

	if (i != (argc - 1))
		goto usage;

	/* Open device */
	path = argv[i];
	while (1) {
		ret = zbc_open(path, flags | O_RDONLY, &dev);
		if (ret != 0)
			return 1;

		zbc_get_device_info(dev, &info);

		printf("Device %s:\n", path);
		zbc_print_device_info(&info, stdout);

		/* We can skip zbc_report_nr_realms() call if we receive
		 * the number from SCSI INQUIRY or ATA LOG page */
		if (info.zbd_realm_list_length == 0) {
			ret = zbc_report_nr_realms(dev, &nr_realms);
			if (ret != 0) {
				fprintf(stderr, "zbc_report_nr_realms failed %d\n",
					ret);
				ret = 1;
				goto out;
			}
		}
		else
			nr_realms = info.zbd_realm_list_length;

		printf("    %u realm%s\n", nr_realms, (nr_realms > 1) ? "s" : "");
		if (num)
			goto out;

		if (!nr || nr > nr_realms)
			nr = nr_realms;
		if (!nr)
			goto out;

		/* Allocate realm array */
		if (!realms)
			realms = (struct zbc_realm *)calloc(nr, sizeof(struct zbc_realm));
		if (!realms) {
			fprintf(stderr, "No memory\n");
			ret = 1;
			goto out;
		}

		/* Get realm information */
		ret = zbc_report_realms(dev, realms, &nr);
		if (ret != 0) {
			if (ret == -EOPNOTSUPP && flags == 0) {
				/* Retry forcing SCSI backend */
				printf("zbc_report_realms returned %d, retrying\n", ret);
				flags = ZBC_O_DRV_SCSI;
				zbc_close(dev);
				continue;
			}
			fprintf(stderr, "zbc_report_realms failed %d\n", ret);
			ret = 1;
			goto out;
		}

		for (i = 0; i < (int)nr; i++) {
			r = &realms[i];
			zbc_report_print_realm(&info, r);
		}

		if (flags != 0)
			break;
	}

out:
	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

