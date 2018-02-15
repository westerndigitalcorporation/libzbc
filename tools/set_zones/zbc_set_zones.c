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
 *         Christophe Louargant (christophe.louargant@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

#include <zbc_private.h>

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	long long conv_num, conv_sz, zone_sz;
	double conv_p;
	int i, ret = -1;
	char *path;

	/* Check command line */
	if (argc < 5) {
usage:
		printf("Usage: %s [options] <dev> <command> <command arguments>\n"
		       "Options:\n"
		       "  -v     : Verbose mode\n"
		       "Commands:\n"
		       "  set_sz <conv zone size (MB)> <zone size (MiB)> :\n"
		       "      Specify the total size in MiB of all conventional\n"
		       "      zones and the size in MiB of zones\n"
		       "  set_ps <conv zone size (%%)> <zone size (MiB)> :\n"
		       "      Specify the percentage of the capacity to use for\n"
		       "      conventional zones and the size in MiB of zones\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < argc - 3; i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (argv[i][0] == '-') {

			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
			return 1;

		} else {

			break;

		}

	}

	if (i > argc - 3)
		goto usage;

	/* Open device: only allow fake device backend driver */
	path = argv[i];
	ret = zbc_open(path, O_RDWR, &dev);
	if (ret < 0) {
		if (ret == -ENXIO)
			fprintf(stderr, "Unsupported device type\n");
		else
			fprintf(stderr, "Open %s failed (%s)\n",
				path, strerror(-ret));
		return 1;
	}

	/* Get device info */
	zbc_get_device_info(dev, &info);
	if (info.zbd_type != ZBC_DT_FAKE) {
		fprintf(stderr,
			"The fake backend driver is not in use for device %s\n",
			path);
		ret = 1;
		goto out;
	}

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);
	printf("\n");

	/* Process command */
	printf("Setting zones:\n");
	i++;

	if (strcmp(argv[i], "set_sz") == 0) {

		/*
		 * Set conventional zones capacity and zone size for all zones.
		 */
		if (i != argc - 3)
			goto usage;

		/* Get arguments */
		conv_sz = (strtoll(argv[i + 1], NULL, 10) * 1024 * 1024) >> 9;
		if (conv_sz < 0) {
			fprintf(stderr, "Invalid conventional zones size %s\n",
				argv[i + 1]);
			ret = 1;
			goto out;
		}

		zone_sz = (strtoll(argv[i + 2], NULL, 10) * 1024 * 1024)
			>> 9;
		if (zone_sz <= 0) {
			fprintf(stderr, "Invalid zone size %s\n",
				argv[i + 2]);
			ret = 1;
			goto out;
		}

	} else if (strcmp(argv[i], "set_ps") == 0) {

		/*
		 * Set conventional zones capacity percentage and
		 * zone size for all zones.
		 */
		if (i != argc - 3)
			goto usage;

		/* Get arguments */
		conv_p = strtof(argv[i + 1], NULL);
		if (conv_p < 0 || conv_p >= 100.0) {
			fprintf(stderr, "Invalid capacity percentage %s for conventional zones\n",
				argv[i + 1]);
			ret = 1;
			goto out;
		}

		zone_sz = (strtoll(argv[i + 2], NULL, 10) * 1024ULL * 1024ULL)
			>> 9;
		if (zone_sz <= 0) {
			fprintf(stderr, "Invalid zone size %s\n",
				argv[i + 2]);
			ret = 1;
			goto out;
		}

		conv_sz = (long long)((double) info.zbd_sectors * (double) conv_p) / 100;
		if (conv_p && conv_sz < zone_sz)
			conv_sz = zone_sz;

	} else {

		fprintf(stderr, "Unknown command \"%s\"\n", argv[i]);
		goto out;

	}

	if ((unsigned long long)conv_sz >= info.zbd_sectors) {
		fprintf(stderr,
			"Invalid conventional zone capacity (too large)\n");
		ret = 1;
		goto out;
	}
	if (conv_sz && conv_sz < zone_sz) {
		fprintf(stderr,
			"Invalid conventional zone capacity (too low)\n");
		ret = 1;
		goto out;
	}

	if (conv_sz) {
		conv_num = conv_sz / zone_sz;
		if (!conv_num || conv_sz % zone_sz)
			conv_num++;
		conv_sz = zone_sz * conv_num;
	} else {
		conv_num = 0;
	}

	printf("    Zone size: %lld MiB (%lld sectors)\n",
	       (zone_sz << 9) / (1024 * 1024),
	       zone_sz);

	printf("    Conventional zones: %lld MiB (%lld sectors), %.02F %% of total capacity), %lld zones\n",
	       (conv_sz << 9) / (1024 * 1024),
	       conv_sz,
	       100.0 * (double)conv_sz / (double)info.zbd_sectors,
	       conv_num);

	printf("    Sequential zones: %llu zones\n",
	       (info.zbd_sectors - conv_sz) / zone_sz);

	ret = zbc_set_zones(dev, conv_sz, zone_sz);
	if (ret != 0) {
		fprintf(stderr,
			"zbc_set_zones failed %d (%s)\n",
			ret,
			strerror(-ret));
		ret = 1;
	}

out:
	zbc_close(dev);

	return ret;
}

