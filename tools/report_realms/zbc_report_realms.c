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
	unsigned int i;

	printf("%03d: domain %u/type 0x%x (%s), act_flgs 0x%x, ",
	       zbc_zone_realm_number(r), zbc_zone_realm_domain(r),
	       zbc_zone_realm_type(r),
	       zbc_zone_type_str(zbc_zone_realm_type(r)),
	       zbc_zone_realm_actv_flags(r));

	for (i = 0; i < zbc_zone_realm_nr_domains(r); i++) {
		printf("%u:[start %lu, end %lu, len %u]",
		       zbc_realm_zone_type(r, i),
		       zbc_sect2lba(info, zbc_realm_start_lba(r, i)),
		       zbc_sect2lba(info, zbc_realm_end_lba(r, i)),
		       zbc_realm_length(r, i));
		printf(i == zbc_zone_realm_nr_domains(r) - 1 ? "\n" : "; ");
	}
	if (i == 0)
		printf("\n");
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int nr_realms = 0, nrlm = 0;
	struct zbc_zone_realm *realms = NULL, *r;
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

			nrlm = strtol(argv[i], NULL, 10);
			if (nrlm <= 0)
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

	if (!nrlm || nrlm > nr_realms)
		nrlm = nr_realms;
	if (!nrlm)
		goto out;

	/* Allocate zone realm descriptor array */
	realms = (struct zbc_zone_realm *)calloc(nrlm,
						 sizeof(struct zbc_zone_realm));
	if (!realms) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	/* Get the realm descriptors */
	ret = zbc_report_realms(dev, realms, &nrlm);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_realms failed %d\n", ret);
		ret = 1;
		goto out;
	}

	for (i = 0, r = realms; i < (int)nrlm; i++, r++)
		zbc_print_realm(&info, r);

out:
	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

