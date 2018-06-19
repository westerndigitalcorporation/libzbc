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

static void zbc_print_domain(struct zbc_device_info *info,
			     struct zbc_zone_domain *d)
{
	printf("%03d: LBA range %014llu:%014llu, type 0x%x (%s)\n",
		zbc_zone_domain_id(d),
		zbc_zone_domain_start_lba(d),
		zbc_zone_domain_end_lba(d),
		zbc_zone_domain_type(d),
		zbc_zone_type_str(zbc_zone_domain_type(d)));
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int nr_domains = 0;
	struct zbc_zone_domain *domains = NULL;
	int i, ret = 1, num = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -n		  : Get only the number of domain descriptors\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-n") == 0) {
			num = 1;
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

	/* Get domain descriptors */
	ret = zbc_list_domains(dev, &domains, &nr_domains);
	if (ret != 0) {
		fprintf(stderr, "zbc_list_domains failed %d\n", ret);
		ret = 1;
		goto out;
	}

	if (num) {
		printf("%u domains\n", nr_domains);
	} else {
		for (i = 0; i < (int)nr_domains; i++)
			zbc_print_domain(&info, &domains[i]);
	}

out:
	if (domains)
		free(domains);
	zbc_close(dev);

	return ret;
}

