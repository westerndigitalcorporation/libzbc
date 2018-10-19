/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public License version 3, "as is," without technical
 * support, and WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
 * received a copy of the GNU Lesser General Public License along with libzbc.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

static void print_zbd_stats(struct zbc_zoned_blk_dev_stats *stats)
{
	printf("\nZoned Block Device Statistics\n"
	       "Maximum Open Zones : %llu\n"
	       "Maximum Explicitly Open SWR and SWP Zones : %llu\n"
	       "Maximum Implicitly Open SWR and SWP Zones : %llu\n"
	       "Maximum Implicitly Open SOBR Zones : %llu\n"
	       "Minimum Empty Zones : %llu\n"
	       "Zones Emptied : %llu\n"
	       "Maximum Non-sequential Zones : %llu\n"
	       "Suboptimal Write Commands : %llu\n"
	       "Commands Exceeding Optimal Limit : %llu\n"
	       "Failed Explicit Opens : %llu\n"
	       "Read Rule Violations : %llx\n"
	       "Write Rule Violations : %llx\n",
	       stats->max_open_zones, stats->max_exp_open_seq_zones,
	       stats->max_imp_open_seq_zones, stats->max_imp_open_sobr_zones,
	       stats->min_empty_zones, stats->zones_emptied,
	       stats->max_non_seq_zones, stats->subopt_write_cmds,
	       stats->cmds_above_opt_lim, stats->failed_exp_opens,
	       stats->read_rule_fails, stats->write_rule_fails);
}

/***** Main *****/

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_device_info info;
	struct zbc_zoned_blk_dev_stats stats;
	bool do_fake = false, do_stats = false;
	int ret, i;

	/* Parse options */
	for (i = 1; i < argc; i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-e") == 0) {

			do_fake = true;

		} else if (strcmp(argv[i], "-s") == 0) {

			do_stats = true;

		} else if (strcmp(argv[i], "--version") == 0) {

			printf("%s\n", zbc_version());
			return 0;

		} else if (argv[i][0] == '-') {

			printf("Unknown option \"%s\"\n",
			       argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (i != (argc - 1)) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "    --version : Print libzbc version\n"
		       "    -v        : Verbose mode\n"
		       "    -s        : Print Zone Block Device statistics (if supported)\n"
		       "    -e        : Print information for an emulated device\n",
		       argv[0]);
		return 1;
	}

	/* Open device */
	ret = zbc_device_is_zoned(argv[i], do_fake, &info);
	if (ret == 1) {
		printf("Device %s:\n", argv[i]);
		zbc_print_device_info(&info, stdout);
		ret = 0;

		if (do_stats) {
			ret = zbc_open(argv[i], O_RDONLY, &dev);
			if (ret != 0) {
				fprintf(stderr, "Open %s failed (%s)\n",
					argv[i], strerror(-ret));
				ret = 1;
			} else {

				ret = zbc_get_zbd_stats(dev, &stats);
				if (ret) {
					fprintf(stderr,
						"%s: Failed to get statistics, err %d (%s)\n",
						argv[0], ret, strerror(-ret));
					ret = 1;
				} else {
					print_zbd_stats(&stats);
				}
			}

			zbc_close(dev);
		}

	} else if (ret == 0) {
		printf("%s: %s is not a zoned block device\n", argv[0], argv[i]);
	} else {
		fprintf(stderr,
			"%s: zbc_device_is_zoned failed %d (%s)\n",
			argv[0], ret, strerror(-ret));
		ret = 1;
	}

	return ret;
}
