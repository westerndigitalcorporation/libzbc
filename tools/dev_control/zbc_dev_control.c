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

static void zbc_print_dhsmr_settings(struct zbc_zp_dev_control *ctl)
{
	printf("FSONZ: %u, SMR Zone: 0x%x, CMR WP Check: %s\n",
	       ctl->zbm_nr_zones, ctl->zbm_smr_zone_type,
	       ctl->zbm_cmr_wp_check ? "Y" : "N");
}

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_device_info info;
	struct zbc_zp_dev_control ctl;
	int i, ret = 1, nz, szt, wpc;
	bool upd = false, set_nz = false, set_szt = false, set_wpc = false;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v		  : Verbose mode\n"
		       "  -nz <num>	  : Set the default number of zones to convert\n"
		       "  -szt <num>	  : Set SMR zone type\n"
		       "  -wpc y|n        : Enable of disable CMR write pointer check\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-nz") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			nz = strtol(argv[i], NULL, 10);
			if (nz <= 0) {
				fprintf(stderr, "invalid -nz value\n");
				goto usage;
			}
			set_nz = true;
		} else if (strcmp(argv[i], "-szt") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			szt = strtol(argv[i], NULL, 10);
			if (szt <= 0 || szt > 255) {
				fprintf(stderr, "invalid -szt value\n");
				goto usage;
			}
			set_szt = true;
		} else if (strcmp(argv[i], "-wpc") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "y") == 0)
				wpc = 1;
			else if (strcmp(argv[i], "n") == 0)
				wpc = 0;
			else {
				fprintf(stderr, "-wpc value must be y or n\n");
				goto usage;
			}
			set_wpc = true;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Unknown option \"%s\"\n",
				argv[i]);
			goto usage;
		} else {
			break;
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

	/* Query the device about persistent DH-SMR settings */
	ret = zbc_zone_activation_ctl(dev, &ctl, false);
	if (ret != 0) {
		fprintf(stderr, "zbc_zone_activation_ctl get failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	if (set_nz) {
		ctl.zbm_nr_zones = nz;
		upd = true;
	}
	if (set_szt) {
		ctl.zbm_smr_zone_type = szt;
		upd = true;
	}
	if (set_wpc) {
		ctl.zbm_cmr_wp_check = wpc ? 0x01 : 0x00;
		upd = true;
	}

	if (upd) {
		/* Need to change some values, request the device to update */
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			fprintf(stderr, "zbc_zone_activation_ctl set failed %d\n",
				ret);
			ret = 1;
			goto out;
		}
	}

	zbc_print_dhsmr_settings(&ctl);

out:
	zbc_close(dev);

	return ret;
}

