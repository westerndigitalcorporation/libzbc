/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
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
	uint64_t start_realm;
	unsigned int nr_realms;
	int i, type, fg = 0, ret = 1, flags = 0;
	char *path;

	/* Check command line */
	if (argc < 5) {
		fprintf(stderr, "Not enough arguments\n");
usage:
		printf("Usage: %s [options] <dev> <start realm> "
		       "<num realms> <new zone type>[ <fg>]\n"
		       "New zone types:\n"
		       "    1             : conventional\n"
		       "    2             : sequential write required\n"
		       "    3             : sequential write preferred\n"
		       "Options:\n"
		       "    -v            : Verbose mode\n"
		       "    -f <hex mask> : Use the specified backend mask"
		       " to open the device\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if ( strcmp(argv[i], "-v") == 0 )
			zbc_set_log_level("debug");
		else if (strcmp(argv[i], "-f") == 0) {
			if (i >= (argc - 1)) {
				fprintf(stderr, "Missing backend flag value\n");
				goto usage;
			}
			i++;

			flags = strtol(argv[i], NULL, 16);
			if (flags != 0) {
				if (flags < 0) {
					fprintf(stderr, "Invalid flag value %i\n",
						flags);
					goto usage;
				}
				flags &= ZBC_O_DRV_MASK;
			}
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
		fprintf(stderr, "Missing starting realm number\n");
		goto usage;
	}
	start_realm = atol(argv[i++]);
	if (i >= argc) {
		fprintf(stderr, "Missing the number of realms to convert\n");
		goto usage;
	}
	nr_realms = atoi(argv[i++]);
	if (i >= argc) {
		fprintf(stderr, "Missing new zone type\n");
		goto usage;
	}
	type = atoi(argv[i++]);
	if (type != ZBC_ZT_CONVENTIONAL &&
	    type != ZBC_ZT_SEQUENTIAL_REQ &&
	    type != ZBC_ZT_SEQUENTIAL_PREF) {
		fprintf(stderr, "Invalid new zone type %i\n", type);
		goto usage;
	}

	if (i < argc)
		fg = atoi(argv[i]);

	/* Open device */
	ret = zbc_open(path, flags | O_RDWR, &dev);
	if (ret != 0)
		return 1;

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	/* Convert realms */
	ret = zbc_convert_realms(dev, start_realm, nr_realms, type, fg);
	if (ret != 0) {
		fprintf(stderr,
			"zbc_convert_realms failed, err %i (%s)\n",
			ret, strerror(-ret));
		ret = 1;
	}

	zbc_close(dev);

	return ret;
}

