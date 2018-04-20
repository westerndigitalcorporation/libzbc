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
 *         Christophe Louargant (christophe.louargant@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

static void zbc_report_print_zone(struct zbc_device_info *info,
				  struct zbc_zone *z,
				  int zno,
				  int lba_unit)
{
	char *start, *length;

	if (lba_unit) {
		start = "block";
		length = "blocks";
	} else {
		start = "sector";
		length = "sectors";
	}

	if (zbc_zone_conv_wp(z)) {
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
		return;
	}

	if (zbc_zone_conventional(z) || zbc_zone_inactive(z)) {
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
		       lba_unit ? zbc_sect2lba(info, zbc_zone_start(z)) :
				  zbc_zone_start(z),
		       lba_unit ? zbc_sect2lba(info, zbc_zone_length(z)) :
				  zbc_zone_length(z),
		       length,
		       lba_unit ? zbc_sect2lba(info, zbc_zone_wp(z)) :
				  zbc_zone_wp(z));
		return;
	}

	printf("Zone %05d: unknown type 0x%x, %s %llu, %llu %s\n",
	       zno,
	       zbc_zone_type(z),
	       start,
	       lba_unit ? zbc_sect2lba(info, zbc_zone_start(z)) :
	       zbc_zone_start(z),
	       lba_unit ? zbc_sect2lba(info, zbc_zone_length(z)) :
	       zbc_zone_length(z),
	       length);
}


int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	unsigned long long sector = 0, nr_sectors = 0;
	enum zbc_reporting_options ro = ZBC_RO_ALL;
	unsigned int nr_zones = 0, nz = 0;
	struct zbc_zone *z, *zones = NULL;
	unsigned int lba_unit = 0;
	unsigned long long start = 0;
	int i, ret = 1;
	int num = 0;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -lba		  : Use LBA size unit (default is 512B sectors)\n"
		       "  -start <offset> : Start offset of report. if \"-lba\" is used\n"
		       "                    <offset> is interpreted as an LBA. Otherwise,\n"
		       "                    it is interpreted as a 512B sector number.\n"
		       "                    Default is 0\n"
		       "  -n		  : Get only the number of zones\n"
		       "  -nz <num>	  : Get at most <num> zones\n"
		       "  -ro <opt>	  : Specify reporting option: \"all\", \"empty\",\n"
		       "                    \"imp_open\", \"exp_open\", \"closed\", \"full\",\n"
		       "                    \"rdonly\", \"offline\", \"rwp\", \"non_seq\" or \"not_wp\".\n"
		       "                    Default is \"all\"\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-n") == 0) {

			num = 1;

		} else if (strcmp(argv[i], "-nz") == 0) {

			if (i >= (argc - 1))
				goto usage;
			i++;

			nz = strtol(argv[i], NULL, 10);
			if (nz <= 0)
				goto usage;

		} else if (strcmp(argv[i], "-lba") == 0) {

			lba_unit = 1;

		} else if (strcmp(argv[i], "-start") == 0) {

			if (i >= (argc - 1))
				goto usage;
			i++;

			start = strtoll(argv[i], NULL, 10);

		} else if (strcmp(argv[i], "-ro") == 0) {

			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "all") == 0) {
				ro = ZBC_RO_ALL;
			} else if (strcmp(argv[i], "empty") == 0) {
				ro = ZBC_RO_EMPTY;
			} else if (strcmp(argv[i], "imp_open") == 0) {
				ro = ZBC_RO_IMP_OPEN;
			} else if (strcmp(argv[i], "exp_open") == 0) {
				ro = ZBC_RO_EXP_OPEN;
			} else if (strcmp(argv[i], "closed") == 0) {
				ro = ZBC_RO_CLOSED;
			} else if (strcmp(argv[i], "full") == 0) {
				ro = ZBC_RO_FULL;
			} else if (strcmp(argv[i], "rdonly") == 0) {
				ro = ZBC_RO_RDONLY;
			} else if (strcmp(argv[i], "offline") == 0) {
				ro = ZBC_RO_OFFLINE;
			} else if (strcmp(argv[i], "inactive") == 0) {
				ro = ZBC_RO_INACTIVE;
			} else if (strcmp(argv[i], "reset") == 0) {
				ro = ZBC_RO_RWP_RECOMMENDED;
			} else if (strcmp(argv[i], "non_seq") == 0) {
				ro = ZBC_RO_NON_SEQ;
			} else if (strcmp(argv[i], "not_wp") == 0) {
				ro = ZBC_RO_NOT_WP;
			} else {
				fprintf(stderr, "Unknown reporting option \"%s\"\n",
					argv[i]);
				goto usage;
			}

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
	path = argv[i];
	ret = zbc_open(path, O_RDONLY, &dev);
	if (ret != 0) {
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

		if (ro == ZBC_RO_ALL) {
			/* Check */
			if (zbc_zone_start(z) != sector) {
				printf("[WARNING] Zone %05d: sector %llu should be %llu\n",
				       i,
				       zbc_zone_start(z), sector);
				sector = zbc_zone_start(z);
			}
			nr_sectors += zbc_zone_length(z);
			sector += zbc_zone_length(z);
		}

		zbc_report_print_zone(&info, z, i, lba_unit);

	}

	if (start == 0 && ro == ZBC_RO_ALL) {
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

}

