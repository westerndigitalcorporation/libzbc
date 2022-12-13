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
#include "zbc_private.h"

static void zbc_print_domain(struct zbc_device *dev,
			     struct zbc_zone_domain *d, bool lba_units)
{
	printf("[ZONE_DOMAIN_INFO],%d,%lu,%lu,%u,0x%x,%s,0x%x\n",
		zbc_zone_domain_id(d),
		lba_units ? zbc_zone_domain_start_lba(dev, d) :
			    zbc_zone_domain_start_sect(d),
		lba_units ? zbc_zone_domain_end_lba(dev, d) :
			    zbc_zone_domain_high_sect(dev, d),
		zbc_zone_domain_nr_zones(d),
		zbc_zone_domain_type(d),
		zbc_zone_type_str(zbc_zone_domain_type(d)),
		zbc_zone_domain_flags(d));
}

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_zone_domain *domains = NULL;
	const char *sk_name, *ascq_name;
	char *path;
	unsigned long long sector = 0LL;
	enum zbc_domain_report_options ro = ZBC_RZD_RO_ALL;
	int i, ret = 1, num = 0;
	unsigned int nr_domains = 0, oflags;
	struct zbc_device_info info;
	struct zbc_err_ext zbc_err;
	bool lba_units = false;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -n		  : Get only the number of domain descriptors\n"
		       "  -ro		  : Reporting options\n"
		       "                  :   all     - Report all zone domains (default)\n"
		       "                  :   allact  - Report all zone domains that for which all zones are active\n"
		       "                  :   act     - Report all zone domains that have active zones\n"
		       "                  :   inact   - Report all zone domains that do not have any active zones\n"
		       "                  :   invalid - Send a reporting option value that is known to be invalid\n"
		       "  -lba            : Use LBA units for output and starting domain locator"
		       "                  : (512B sectors are unsed by default)\n"
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
		} else if (strcmp(argv[i], "-n") == 0) {
			num = 1;
		} else if (strcmp(argv[i], "-start") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			sector = strtoll(argv[i], NULL, 10);
		} else if (strcmp(argv[i], "-lba") == 0) {
			lba_units = true;
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
			} else if (strcmp(argv[i], "invalid") == 0) {
				ro = 0x15;
			} else {
				fprintf(stderr, "Unknown reporting option \"%s\"\n",
					argv[i]);
				goto usage;
			}
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
	oflags = ZBC_O_DEVTEST;
	oflags |= ZBC_O_DRV_ATA;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(path, oflags | O_RDONLY, &dev);
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

	if (lba_units)
		sector = zbc_lba2sect(&info, sector);

	if (!zbc_device_is_zdr(&info)) {
		fprintf(stderr,
			"[TEST][ERROR],not a ZDR device\n");
		ret = 1;
		goto out;
	}

	/* Get domain descriptors */
	ret = zbc_list_domains(dev, sector, ro, &domains, &nr_domains);
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
			zbc_print_domain(dev, &domains[i], lba_units);
	}

out:
	if (domains)
		free(domains);
	zbc_close(dev);

	return ret;
}
