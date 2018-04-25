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
				    struct zbc_cvt_domain *d)
{
	if (zbc_cvt_domain_conventional(d) || zbc_cvt_domain_wpc(d) ||
	    zbc_cvt_domain_sequential(d) ||zbc_cvt_domain_seq_pref(d)) {
		printf("%03d: type 0x%x (%s), conv LBA %08llu:"
		       "%u zones, seq LBA %08llu:%u zones, kpo %u, "
		       "cvt to conv: %s, cvt to seq: %s\n",
		       zbc_cvt_domain_number(d), zbc_cvt_domain_type(d),
		       zbc_zone_type_str(zbc_cvt_domain_type(d)),
		       zbc_cvt_domain_conv_start(d),
		       zbc_cvt_domain_conv_length(d),
		       zbc_cvt_domain_seq_start(d),
		       zbc_cvt_domain_seq_length(d),
		       zbc_cvt_domain_keep_out(d),
		       zbc_cvt_domain_to_conv(d) ? "Y" : "N",
		       zbc_cvt_domain_to_seq(d) ? "Y" : "N");
		return;
	}

	printf("Conversion domain %03d: unknown type 0x%x\n",
	       zbc_cvt_domain_number(d), zbc_cvt_domain_type(d));
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned int nr_domains = 0, nd = 0;
	struct zbc_cvt_domain *domains = NULL;
	int i, ret = 1, num = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -n		  : Get only the number of domain descriptors\n"
		       "  -nd <num>	  : Get at most <num> domain descriptors\n",
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

	ret = zbc_report_nr_domains(dev, &nr_domains);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_nr_domains failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	printf("    %u conversion domain%s\n",
	       nr_domains, (nr_domains > 1) ? "s" : "");
	if (num)
		goto out;

	if (!nd || nd > nr_domains)
		nd = nr_domains;
	if (!nd)
		goto out;

	/* Allocate conversion domain descriptor array */
	domains = (struct zbc_cvt_domain *)calloc(nd,
						 sizeof(struct zbc_cvt_domain));
	if (!domains) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	/* Get the domain descriptors */
	ret = zbc_domain_report(dev, domains, &nd);
	if (ret != 0) {
		fprintf(stderr, "zbc_domain_report failed %d\n", ret);
		ret = 1;
		goto out;
	}

	for (i = 0; i < (int)nd; i++)
		zbc_print_domain(&info, &domains[i]);

out:
	if (domains)
		free(domains);
	zbc_close(dev);

	return ret;
}

