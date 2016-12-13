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
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	zbc_zone_t *zones = NULL, *rzone = NULL;;
	long long start = 0;
	unsigned long long start_sector;
	unsigned int flags = 0;
	int i, ret = 1;
	unsigned int nr_zones, rzone_idx = -1;
	bool sector_unit = false;
	bool lba_unit = false;
	char *path;

	/* Check command line */
	if ( argc < 2 ) {
usage:
		printf("Usage: %s [options] <dev> <zone>\n"
		       "  By default <zone> is interpreted as a zone number.\n"
		       "  If the -lba option is used, <zone> is interpreted\n"
		       "  as the start LBA of the zone to reset. If the\n"
		       "  -sector option is used, <zone> is interpreted as\n"
		       "  the start 512B sector of the zone to reset.\n"
		       "Options:\n"
		       "  -v      : Verbose mode\n"
		       "  -sector : Interpret <zone> as a zone start sector\n"
		       "  -lba    : Interpret <zone> as a zone start LBA\n"
		       "  -all    : Reset all sequential zones\n",
		       argv[0]);
		return( 1 );
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-sector") == 0) {

			sector_unit = true;

		} else if (strcmp(argv[i], "-lba") == 0) {

			lba_unit = true;

		} else if (strcmp(argv[i], "-all") == 0) {

			flags |= ZBC_OP_ALL_ZONES;

		} else if ( argv[i][0] == '-' ) {

			printf("Unknown option \"%s\"\n",
			       argv[i]);
			goto usage;

		} else {

			break;

		}

	}

	if (lba_unit && sector_unit) {
		fprintf(stderr, "-lba and -sector cannot be used together\n");
		return 1;
	}

	if (i != (argc - 2))
		goto usage;

	/* Open device */
	path = argv[i];
	ret = zbc_open(path, O_RDWR, &dev);
	if (ret != 0)
		return 1;

	zbc_get_device_info(dev, &info);

	printf("Device %s: %s\n",
	       path,
	       info.zbd_vendor_id);
	printf("    %s interface, %s disk model\n",
	       zbc_disk_type_str(info.zbd_type),
	       zbc_disk_model_str(info.zbd_model));
	printf("    %llu 512-bytes sectors\n",
	       (unsigned long long) info.zbd_sectors);
	printf("    %llu logical blocks of %u B\n",
	       (unsigned long long)info.zbd_lblocks,
	       (unsigned int)info.zbd_lblock_size);
	printf("    %llu physical blocks of %u B\n",
	       (unsigned long long)info.zbd_pblocks,
	       (unsigned int)info.zbd_pblock_size);
	printf("    %.03F GB capacity\n",
	       (double)(info.zbd_sectors << 9) / 1000000000);

	/* Target zone */
	start = strtoll(argv[i + 1], NULL, 10);
	if (start < 0) {
		fprintf(stderr, "Invalid zone\n");
		ret = 1;
		goto out;
	}
	if (lba_unit)
		start_sector = zbc_lba2sect(&info, start);
	else if (sector_unit)
		start_sector = start;
	else
		start_sector = 0;

	if (flags & ZBC_OP_ALL_ZONES) {

		printf("Resetting all zones...\n");
		start_sector = 0;

	} else {

		/* Get zone list */
		ret = zbc_list_zones(dev, start, ZBC_RO_ALL, &zones, &nr_zones);
		if ( ret != 0 ) {
			fprintf(stderr, "zbc_list_zones failed\n");
			ret = 1;
			goto out;
		}

		if (lba_unit || sector_unit) {
			/* Search target zone */
			for (i = 0; i < (int)nr_zones; i++) {
				if (start_sector >= zbc_zone_start(&zones[i]) &&
				    start_sector < zbc_zone_start(&zones[i]) + zbc_zone_length(&zones[i])) {
					rzone = &zones[i];
					rzone_idx = i;
					break;
				}
			}
		} else if (start < nr_zones) {
			rzone = &zones[start];
			rzone_idx = start;
		}
		if (!rzone) {
			fprintf(stderr, "Target zone not found\n");
			ret = 1;
			goto out;
		}

		if (lba_unit)
			printf("Resetting zone %d/%d, LBA %llu...\n",
			       rzone_idx, nr_zones,
			       (unsigned long long)zbc_sect2lba(&info, zbc_zone_start(rzone)));
		else
			printf("Resetting zone %d/%d, sector %llu...\n",
			       rzone_idx, nr_zones,
			       (unsigned long long)zbc_zone_start(rzone));

		start_sector = zbc_zone_start(rzone);

	}

	/* Reset zone(s) */
	ret = zbc_reset_zone(dev, start_sector, flags);
	if (ret != 0) {
		fprintf(stderr, "zbc_reset_zone failed\n");
		ret = 1;
	}

out:

	if (zones)
		free(zones);
	zbc_close(dev);

	return ret;

}

