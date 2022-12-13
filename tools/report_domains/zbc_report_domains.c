/*
 * SPDX-License-Identifier: BSD-2-Clause
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Copyright (c) 2023 Western Digital Corporation or its affiliates.
 *
 * This file is part of libzbc.
 *
 * Author: Dmitry Fomichev (dmitry.fomichev@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

static void zbc_print_domain(struct zbc_device *dev,
			     struct zbc_zone_domain *d, bool lba_units)
{
	unsigned short dom_flgs = zbc_zone_domain_flags(d);

	printf("%03d: %s range %014lu:%014lu, %u zones, type 0x%x (%s), flags 0x%x (VALID ZONE TYPE : %s, SHIFTING REALM BOUNDARIES : %s)\n",
		zbc_zone_domain_id(d),
		lba_units ? "lblock" : "sector",
		lba_units ? zbc_zone_domain_start_lba(dev, d) :
			    zbc_zone_domain_start_sect(d),
		lba_units ? zbc_zone_domain_end_lba(dev, d) :
			    zbc_zone_domain_high_sect(dev, d),
		zbc_zone_domain_nr_zones(d),
		zbc_zone_domain_type(d),
		zbc_zone_type_str(zbc_zone_domain_type(d)),
		dom_flgs,
		(dom_flgs & ZBC_ZDF_VALID_ZONE_TYPE) ? "Y" : "N",
		(dom_flgs & ZBC_ZDF_SHIFTING_BOUNDARIES) ? "Y" : "N");
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	char *path, *end;
	unsigned long long sector = 0LL;
	unsigned int nr_domains = 0;
	struct zbc_zone_domain *domains = NULL;
	int i, ret = 1, num = 0, oflags = 0;
	enum zbc_domain_report_options ro = ZBC_RZD_RO_ALL;
	bool lba_units = false;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v              : Verbose mode\n"
		       "  -scsi           : Force the use of SCSI passthrough commands\n"
		       "  -ata            : Force the use of ATA passthrough commands\n"
		       "  -lba            : Use LBA units for output and starting domain locator"
		       "                  : (512B sectors are used by default)\n"
		       "  -n              : Get only the number of domain descriptors\n"
		       "  -ro             : Reporting options\n"
		       "                  :   all    - Report all zone domains (default)\n"
		       "                  :   allact - Report all zone domains that for which all zones are active\n"
		       "                  :   act    - Report all zone domains that have active zones\n"
		       "                  :   inact  - Report all zone domains that do not have any active zones\n"
		       "  -start          : Start sector /LBA to report (0 by default)\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {
		if (argv[i][0] != '-')
			break;
		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-scsi") == 0) {
			oflags = ZBC_O_DRV_SCSI;
		} else if (strcmp(argv[i], "-ata") == 0) {
			oflags = ZBC_O_DRV_ATA;
		} else if (strcmp(argv[i], "-lba") == 0) {
			lba_units = true;
		} else if (strcmp(argv[i], "-n") == 0) {
			num = 1;
		} else if (strcmp(argv[i], "-start") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			sector = strtoll(argv[i], &end, 10);
			if (*end != '\0')
				goto usage;
		} else if (strcmp(argv[i], "-ro") == 0) {

			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "all") == 0) {
				ro = ZBC_RZD_RO_ALL;
			} else if (strcmp(argv[i], "allact") == 0) {
				ro = ZBC_RZD_RO_ALL_ACTIVE;
			} else if (strcmp(argv[i], "act") == 0) {
				ro = ZBC_RZD_RO_ACTIVE;
			} else if (strcmp(argv[i], "inact") == 0) {
				ro = ZBC_RZD_RO_INACTIVE;
			} else {
				fprintf(stderr, "Unknown reporting option \"%s\"\n",
					argv[i]);
				goto usage;
			}
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
	ret = zbc_open(path, oflags | O_RDONLY, &dev);
	if (ret != 0) {
		if (ret == -ENODEV)
			fprintf(stderr,
				"Open %s failed (not a zoned block device)\n",
				path);
		else
			fprintf(stderr, "Open %s failed (%s)\n",
				path, strerror(-ret));
		return 1;
	}

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	if (lba_units)
		sector = zbc_lba2sect(&info, sector);

	/* Get domain descriptors */
	ret = zbc_list_domains(dev, sector, ro, &domains, &nr_domains);
	if (ret != 0) {
		fprintf(stderr, "zbc_list_domains failed %d\n", ret);
		ret = 1;
		goto out;
	}

	if (num) {
		printf("%u domains\n", nr_domains);
	} else {
		for (i = 0; i < (int)nr_domains; i++)
			zbc_print_domain(dev, &domains[i], lba_units);
	}

out:
	if (domains)
		free(domains);
	zbc_close(dev);

	return ret;
}

