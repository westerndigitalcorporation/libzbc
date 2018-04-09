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
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

#define ZBC_O_DRV_MASK (ZBC_O_DRV_BLOCK | ZBC_O_DRV_SCSI | ZBC_O_DRV_ATA)

static void zbc_print_zone_activation_settings(struct zbc_zp_dev_control *ctl)
{
	printf("FSONZ: %u, CMR WP Check: %s\n",
	       ctl->zbm_nr_zones, ctl->zbm_cmr_wp_check ? "Y" : "N");
}

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_device_info info;
	struct zbc_zp_dev_control ctl;
	enum zbc_mutation_target mt = ZBC_MT_UNKNOWN;
	int i, ret = 1, nz = 0;
	bool upd = false, wp_check = false, set_nz = false, set_wp_chk = false;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v              : Verbose mode\n"
		       "  -mu <to>        : Mutate to the specified target\n"
		       "  -nz <num>       : Set the default number of zones to convert\n"
		       "  -wpc y|n        : Enable of disable CMR write pointer check\n\n"
		       "Mutation targets:\n"
		       "  PMR             : A classic, not zoned, device\n"
		       "  HM              : Host-managed SMR device\n"
		       "  HA              : Host-aware SMR device\n"
		       "  ZA              : DH-SMR device supporting Zone Activation"
		       " command set, no CMR-only zones\n"
		       "  ZABD            : Same as ZA, but the first conversion domain"
		       " is CMR-only\n"
		       "  ZABDTD          : Same as ZA, but the first and last conversion"
		       " domains are CMR-only\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-mu") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;
			if (isdigit(argv[i][0]))
				mt = strtol(argv[i], NULL, 0);
			else if (strcmp(argv[i], "PMR") == 0)
				mt = ZBC_MT_NON_ZONED;
			else if (strcmp(argv[i], "HM") == 0)
				mt = ZBC_MT_HM_ZONED;
			else if (strcmp(argv[i], "HA") == 0)
				mt = ZBC_MT_HA_ZONED;
			else if (strcmp(argv[i], "ZA") == 0)
				mt = ZBC_MT_ZA_NO_CMR;
			else if (strcmp(argv[i], "ZABD") == 0)
				mt = ZBC_MT_ZA_1_CMR_BOT;
			else if (strcmp(argv[i], "ZABDTD") == 0)
				mt = ZBC_MT_ZA_1_CMR_BOT_TOP;
			if (mt == ZBC_MT_UNKNOWN) {
				fprintf(stderr, "unknown mutation target %s\n",
					argv[i]);
				goto usage;
			}
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
		} else if (strcmp(argv[i], "-wpc") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "y") == 0)
				wp_check = true;
			else if (strcmp(argv[i], "n") == 0)
				wp_check = false;
			else {
				fprintf(stderr, "-wpc value must be y or n\n");
				goto usage;
			}
			set_wp_chk = true;
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

	if (mt != ZBC_MT_UNKNOWN) {
		if (!(info.zbd_flags & ZBC_MUTATE_SUPPORT)) {
			fprintf(stderr, "Device doesn't support MUTATE\n");
			ret = 1;
			goto out;
		}

		/* Try to mutate the device */
		ret = zbc_mutate(dev, mt);
		if (ret != 0) {
			fprintf(stderr, "zbc_mutate failed %d\n", ret);
			ret = 1;
			goto out;
		}

		if (mt == ZBC_MT_NON_ZONED)
			return 0;

		/* Need to reopen the device to receive the updated info */
		zbc_close(dev);
		ret = zbc_open(path, ZBC_O_DRV_MASK | O_RDONLY, &dev);
		if (ret != 0)
			return 1;

		zbc_get_device_info(dev, &info);
	}

	if (!(info.zbd_flags & ZBC_ZONE_ACTIVATION_SUPPORT)) {
		if (set_nz || set_wp_chk) {
			fprintf(stderr, "Not a Zone Activation device\n");
			ret = 1;
		}
		goto out;
	}

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
	if (set_wp_chk) {
		ctl.zbm_cmr_wp_check = wp_check ? 0x01 : 0x00;
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

	zbc_print_zone_activation_settings(&ctl);

out:
	zbc_close(dev);

	return ret;
}

