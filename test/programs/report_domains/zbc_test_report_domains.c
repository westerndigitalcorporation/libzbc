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
	printf("[ZONE_DOMAIN_INFO],%d,%llu,%llu,0x%x,%s\n",
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
	const char *sk_name, *ascq_name;
	char *path;
	struct zbc_err_ext zbc_err;

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
			fprintf(stderr, "[TEST][ERROR],Unknown option \"%s\"\n",
				argv[i]);
			goto usage;
		}
	}

	if (i != (argc - 1))
		goto usage;
	path = argv[i];

	/* Open device */
	ret = zbc_open(path, ZBC_O_DRV_MASK | O_RDONLY, &dev);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],open device failed, err %i (%s) %s\n",
			ret, strerror(-ret), path);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	if (!(info.zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT)) {
		fprintf(stderr,
			"[TEST][ERROR],not a Zone Domains device\n");
		ret = 1;
		goto out;
	}

	/* Get domain descriptors */
	ret = zbc_list_domains(dev, &domains, &nr_domains);
	if (ret != 0) {
		zbc_errno_ext(dev, &zbc_err, sizeof(zbc_err));
		sk_name = zbc_sk_str(zbc_err.sk);
		ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);
		fprintf(stderr,
			"[TEST][ERROR],zbc_list_domains failed, err %i (%s)\n",
			ret, strerror(-ret));
		printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
		printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
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

