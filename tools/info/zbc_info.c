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

/***** Main *****/

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	bool do_fake = false;
	int ret, i;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "    -v : Verbose mode\n"
		       "    -e : Print information for an emulated device\n",
		       argv[0]);
		return 1;
	}

	/* Be silent by default */
	zbc_set_log_level("none");

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-e") == 0) {

			do_fake = true;

		} else if (argv[i][0] == '-') {

			printf("Unknown option \"%s\"\n",
			       argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (i != (argc - 1))
		goto usage;

	/* Open device */
	ret = zbc_device_is_zoned(argv[i], do_fake, &info);
	if (ret == 1) {
		printf("Device %s:\n", argv[i]);
		zbc_print_device_info(&info, stdout);
		ret = 0;
	} else if (ret == 0) {
		printf("%s is not a zoned block device\n", argv[i]);
	} else {
		fprintf(stderr,
			"zbc_device_is_zoned failed %d (%s)\n",
			ret, strerror(-ret));
		ret = 1;
	}

	return ret;
}
