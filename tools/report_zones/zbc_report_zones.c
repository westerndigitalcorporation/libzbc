// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 *         Christophe Louargant (christophe.louargant@wdc.com)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <libzbc/zbc.h>

static inline unsigned long long zbc_report_val(struct zbc_device_info *info,
						unsigned long long val,
						bool lba_unit)
{
	return lba_unit ? zbc_sect2lba(info, val) : val;
}

static void zbc_report_print_zone(struct zbc_device_info *info,
				  struct zbc_zone *z, int zno,
				  bool lba_unit)
{
	char const *start, *length;

	if (lba_unit) {
		start = "block";
		length = "blocks";
	} else {
		start = "sector";
		length = "sectors";
	}

	if (zbc_zone_sobr(z)) {
		if (zbc_zone_condition(z) == ZBC_ZC_IMP_OPEN ||
		    zbc_zone_condition(z) == ZBC_ZC_EMPTY) {
			printf("Zone %05d: type 0x%x (%s), cond 0x%x (%s), %s %llu, "
			       "%llu %s, wp %llu\n",
			       zno,
			       zbc_zone_type(z),
			       zbc_zone_type_str(zbc_zone_type(z)),
			       zbc_zone_condition(z),
			       zbc_zone_condition_str(zbc_zone_condition(z)),
			       start,
			       lba_unit ? zbc_sect2lba(info, zbc_zone_start(z)) :
					  zbc_zone_start(z),
			       lba_unit ? zbc_sect2lba(info, zbc_zone_length(z)) :
					  zbc_zone_length(z),
			       length,
			       lba_unit ? zbc_sect2lba(info, zbc_zone_wp(z)) :
					  zbc_zone_wp(z));
		} else {
			printf("Zone %05d: type 0x%x (%s), cond 0x%x (%s), %s %llu, "
			       "%llu %s\n",
			       zno,
			       zbc_zone_type(z),
			       zbc_zone_type_str(zbc_zone_type(z)),
			       zbc_zone_condition(z),
			       zbc_zone_condition_str(zbc_zone_condition(z)),
			       start,
			       lba_unit ? zbc_sect2lba(info, zbc_zone_start(z)) :
					  zbc_zone_start(z),
			       lba_unit ? zbc_sect2lba(info, zbc_zone_length(z)) :
					  zbc_zone_length(z),
			       length);
		}
		return;
	}

	if (zbc_zone_conventional(z) || zbc_zone_inactive(z) || zbc_zone_gap(z)) {
		printf("Zone %05d: type 0x%x (%s), cond 0x%x (%s), %s %llu, "
		       "%llu %s\n",
		       zno,
		       zbc_zone_type(z),
		       zbc_zone_type_str(zbc_zone_type(z)),
		       zbc_zone_condition(z),
		       zbc_zone_condition_str(zbc_zone_condition(z)),
		       start,
		       zbc_report_val(info, zbc_zone_start(z), lba_unit),
		       zbc_report_val(info, zbc_zone_length(z), lba_unit),
		       length);
		return;
	}

	if (zbc_zone_sequential(z)) {
		printf("Zone %05d: type 0x%x (%s), cond 0x%x (%s), reset "
		       "recommended %d, non_seq %d, %s %llu, %llu %s, wp %llu\n",
		       zno,
		       zbc_zone_type(z),
		       zbc_zone_type_str(zbc_zone_type(z)),
		       zbc_zone_condition(z),
		       zbc_zone_condition_str(zbc_zone_condition(z)),
		       zbc_zone_rwp_recommended(z),
		       zbc_zone_non_seq(z),
		       start,
		       zbc_report_val(info, zbc_zone_start(z), lba_unit),
		       zbc_report_val(info, zbc_zone_length(z), lba_unit),
		       length,
		       zbc_report_val(info, zbc_zone_wp(z), lba_unit));
		return;
	}

	printf("Zone %05d: unknown type 0x%x, %s %llu, %llu %s\n",
	       zno,
	       zbc_zone_type(z),
	       start,
	       zbc_report_val(info, zbc_zone_start(z), lba_unit),
	       zbc_report_val(info, zbc_zone_length(z), lba_unit),
	       length);
}


static int zbc_report_zones_usage(FILE *out, char *prog)
{
	fprintf(out,
		"Usage: %s [options] <dev>\n"
		"Options:\n"
		"  -h | --help   : Display this help message and exit\n"
		"  -v		 : Verbose mode\n"
		"  -scsi         : Force the use of SCSI passthrough commands\n"
		"  -ata          : Force the use of ATA passthrough commands\n"
		"  -lba          : Use LBA size unit (default is 512B sectors)\n"
		"  -start <ofst> : Start offset of the report. if -lba is\n"
		"                  specified, <ofst> is interpreted as an LBA\n"
		"                  value. Otherwise, it is interpreted as a\n"
		"                  512B sector value. Default is 0\n"
		"  -n            : Get only the number of zones in the report\n"
		"  -nz <num>     : Report at most <num> zones\n"
		"  -ro <opt>     : Specify a reporting option. <opt> can be:\n"
		"                  - all: report all zones (default)\n"
		"                  - empty: report only empty zones\n"
		"                  - imp_open: report only implicitly open zones\n"
		"                  - exp_open: report only explicitly open zones\n"
		"                  - closed: report only closed zones\n"
		"                  - full: report only full zones\n"
		"                  - rdonly: report only read-only zones\n"
		"                  - offline: report only offline zones\n"
		"                  - inactive: report only inactive zones\n"
		"                  - rwp: report only offline zones\n"
		"                  - non_seq: report only offline zones\n"
		"                  - not_wp: report only zones that are not\n"
		"                    write pointer zones (e.g. conventional zones)\n",
		basename(prog));

	return 1;
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned long long sector = 0, nr_sectors = 0;
	enum zbc_zone_reporting_options ro = ZBC_RZ_RO_ALL;
	unsigned int nr_zones = 0, nz = 0;
	struct zbc_zone *z, *zones = NULL;
	bool lba_unit = false;
	unsigned long long start = 0;
	int i, ret = 1, oflags = 0;
	int num = 0;
	char *path, *end;

	/* Check command line */
	if (argc < 2)
		return zbc_report_zones_usage(stderr, argv[0]);

	/* Parse options */
	for (i = 1; i < argc; i++) {

		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return zbc_report_zones_usage(stdout, argv[0]);

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-scsi") == 0) {

			oflags = ZBC_O_DRV_SCSI;

		} else if (strcmp(argv[i], "-ata") == 0) {

			oflags = ZBC_O_DRV_ATA;

		} else if (strcmp(argv[i], "-n") == 0) {

			num = 1;

		} else if (strcmp(argv[i], "-nz") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			nz = strtol(argv[i], &end, 10);
			if (*end != '\0' || nz == 0) {
				fprintf(stderr, "Missing -nz value\n");
				return 1;
			}

		} else if (strcmp(argv[i], "-lba") == 0) {

			lba_unit = true;

		} else if (strcmp(argv[i], "-start") == 0) {

			if (i >= (argc - 1)) {
				printf("Missing -start value\n");
				return 1;
			}
			i++;

			start = strtoll(argv[i], &end, 10);
			if (*end != '\0') {
				fprintf(stderr, "Invalid start offset \"%s\"\n",
					argv[i]);
				return 1;
			}

		} else if (strcmp(argv[i], "-ro") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			if (strcmp(argv[i], "all") == 0) {
				ro = ZBC_RZ_RO_ALL;
			} else if (strcmp(argv[i], "empty") == 0) {
				ro = ZBC_RZ_RO_EMPTY;
			} else if (strcmp(argv[i], "imp_open") == 0) {
				ro = ZBC_RZ_RO_IMP_OPEN;
			} else if (strcmp(argv[i], "exp_open") == 0) {
				ro = ZBC_RZ_RO_EXP_OPEN;
			} else if (strcmp(argv[i], "closed") == 0) {
				ro = ZBC_RZ_RO_CLOSED;
			} else if (strcmp(argv[i], "full") == 0) {
				ro = ZBC_RZ_RO_FULL;
			} else if (strcmp(argv[i], "rdonly") == 0) {
				ro = ZBC_RZ_RO_RDONLY;
			} else if (strcmp(argv[i], "offline") == 0) {
				ro = ZBC_RZ_RO_OFFLINE;
			} else if (strcmp(argv[i], "inactive") == 0) {
				ro = ZBC_RZ_RO_INACTIVE;
			} else if (strcmp(argv[i], "reset") == 0) {
				ro = ZBC_RZ_RO_RWP_RECMND;
			} else if (strcmp(argv[i], "non_seq") == 0) {
				ro = ZBC_RZ_RO_NON_SEQ;
			} else if (strcmp(argv[i], "not_wp") == 0) {
				ro = ZBC_RZ_RO_NOT_WP;
			} else if (strcmp(argv[i], "gap") == 0) {
				ro = ZBC_RZ_RO_GAP;
			} else {
				fprintf(stderr, "Unknown reporting option \"%s\"\n",
					argv[i]);
				return 1;
			}

		} else if (argv[i][0] == '-') {

			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			return 1;

		} else {

			break;

		}

	}

	if (i != (argc - 1)) {
		fprintf(stderr, "No device specified\n");
		return 1;
	}

	if (oflags & ZBC_O_DRV_SCSI && oflags & ZBC_O_DRV_ATA) {
		fprintf(stderr,
			"-scsi and -ata options are mutually exclusive\n");
		return 1;
	}

	/* Open device */
	path = argv[i];
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

	/* Get the number of zones */
	if (lba_unit)
		sector = zbc_lba2sect(&info, start);
	else
		sector = start;
	ret = zbc_report_nr_zones(dev, sector, ro, &nr_zones);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_nr_zones at %llu, ro 0x%02x failed %d\n",
			start, (unsigned int) ro, ret);
		ret = 1;
		goto out;
	}

	/* Print zone info */
	printf("    %u zone%s from %llu, reporting option 0x%02x\n",
	       nr_zones,
	       (nr_zones > 1) ? "s" : "",
	       start, ro);

	if (num)
		goto out;

	if (!nz || nz > nr_zones)
		nz = nr_zones;
	if (!nz)
		goto out;

	/* Allocate zone array */
	zones = (struct zbc_zone *) calloc(nz, sizeof(struct zbc_zone));
	if (!zones) {
		fprintf(stderr, "No memory\n");
		ret = 1;
		goto out;
	}

	/* Get zone information */
	ret = zbc_report_zones(dev, sector, ro, zones, &nz);
	if (ret != 0) {
		fprintf(stderr, "zbc_report_zones failed %d\n", ret);
		ret = 1;
		goto out;
	}

	printf("%u / %u zone%s:\n", nz, nr_zones, (nz > 1) ? "s" : "");
	if (lba_unit)
		sector = zbc_lba2sect(&info, start);
	else
		sector = start;
	for (i = 0; i < (int)nz; i++) {

		z = &zones[i];

		if (ro == ZBC_RZ_RO_ALL) {
			/* Check */
			if (zbc_zone_start(z) != sector) {
				printf("[WARNING] Zone %05d: sector %llu "
				       "should be %llu\n",
				       i, zbc_zone_start(z), sector);
				sector = zbc_zone_start(z);
			}
			nr_sectors += zbc_zone_length(z);
			sector += zbc_zone_length(z);
		}

		zbc_report_print_zone(&info, z, i, lba_unit);

	}

	if (start == 0 && ro == ZBC_RZ_RO_ALL && nz == nr_zones) {
		/* Check */
		if ( zbc_sect2lba(&info, nr_sectors) != info.zbd_lblocks ) {
			printf("[WARNING] %llu logical blocks reported "
			       "but capacity is %llu logical blocks\n",
			       zbc_sect2lba(&info, nr_sectors),
			       (unsigned long long)info.zbd_lblocks);
		}
	}

out:
	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;

err:
	fprintf(stderr, "Invalid command line\n");

	return 1;
}

