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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	struct zbc_zone_realm *realms = NULL, *r;
	char *path, *end;
	unsigned long long sector = 0LL;
	unsigned int nr_realms = 0, nrlm = 0;
	int i, ret = 1, num = 0, oflags = 0;
	enum zbc_realm_report_options ro = ZBC_RR_RO_ALL;
	bool lba_units = false;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v              : Verbose mode\n"
		       "  -scsi           : Force the use of SCSI passthrough commands\n"
		       "  -ata            : Force the use of ATA passthrough commands\n"
		       "  -lba            : Use logical block units (512B sectors are used by default)\n"
		       "  -n              : Get only the number of realm descriptors\n"
		       "  -nd <num>       : Get at most <num> realm descriptors\n"
		       "  -ro             : Realm reporting options:\n"
		       "                  :   all  - Report all realms (default)\n"
		       "                  :   sobr - Report all realms that contain active SOBR zones\n"
		       "                  :   seq  - Report all realms that contain active SWR zones\n"
		       "                  :   seqp - Report all realms that contain active SWP zones\n"
		       "  -start          : Realm locator sector/LBA (0 by default)\n",
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
		} else if (strcmp(argv[i], "-n") == 0) {
			num = 1;
		} else if (strcmp(argv[i], "-ro") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "all") == 0) {
				ro = ZBC_RR_RO_ALL;
			} else if (strcmp(argv[i], "sobr") == 0) {
				ro = ZBC_RR_RO_SOBR;
			} else if (strcmp(argv[i], "seq") == 0) {
				ro = ZBC_RR_RO_SWR;
			} else if (strcmp(argv[i], "seqp") == 0) {
				ro = ZBC_RR_RO_SWP;
			} else {
				fprintf(stderr, "Unknown reporting option \"%s\"\n",
					argv[i]);
				goto usage;
			}
		} else if (strcmp(argv[i], "-start") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			sector = strtoll(argv[i], &end, 10);
			if (*end != '\0')
				goto usage;
		} else if (strcmp(argv[i], "-lba") == 0) {
			lba_units = true;
		} else if (strcmp(argv[i], "-nd") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			nrlm = strtol(argv[i], &end, 10);
			if (*end != '\0' || nrlm <= 0)
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
	ret = zbc_report_realms(dev, sector, ro, realms, &nrlm);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_realms failed %d\n", ret);
		ret = 1;
		goto out;
	}

	for (i = 0, r = realms; i < (int)nrlm; i++, r++) {
		unsigned int j;

		printf("%03d: domain %u/type 0x%x (%s), act_flgs 0x%x, restr 0x%x, ",
		       zbc_zone_realm_number(r), zbc_zone_realm_domain(r),
		       zbc_zone_realm_type(r),
		       zbc_zone_type_str(zbc_zone_realm_type(r)),
		       zbc_zone_realm_actv_flags(r),
		       zbc_zone_realm_restrictions(r));

		for (j = 0; j < zbc_zone_realm_nr_domains(r); j++) {
			printf("%u:[start %"PRIu64", end %"PRIu64", %u zones/%"PRIu64" %s]",
			       zbc_realm_zone_type(r, j),
				lba_units ? zbc_realm_start_lba(dev, r, j) :
					    zbc_realm_start_sector(r, j),
				lba_units ? zbc_realm_end_lba(dev, r, j) :
					    zbc_realm_high_sector(dev, r, j),
				zbc_realm_length(r, j),
				lba_units ? zbc_realm_lblock_length(dev, r, j) :
					    zbc_realm_sector_length(r, j),
				lba_units ? "lblocks" : "sectors");
			printf(j == zbc_zone_realm_nr_domains(r) - 1 ? "\n" : "; ");
		}
		if (j == 0)
			printf("\n");
	}
out:
	if (realms)
		free(realms);
	zbc_close(dev);

	return ret;
}

