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
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

static void zbc_print_zone_activation_settings(struct zbc_zd_dev_control *ctl)
{
	printf("    FSNOZ: %u, URSWRZ: %s, MAX ACTIVATION: %u\n",
	       ctl->zbt_nr_zones, ctl->zbt_urswrz ? "Y" : "N", ctl->zbt_max_activate);
}

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_device_info info;
	struct zbc_zd_dev_control ctl;
	int i, ret = 1, oflags = 0,nz = 0, max_activate = 0;
	bool upd = false, urswrz = false, set_nz = false;
	bool set_urswrz = false, set_max_activate = false;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v                        : Verbose mode\n"
		       "  -scsi                     : Force the use of SCSI passthrough commands\n"
		       "  -ata                      : Force the use of ATA passthrough commands\n"
		       "  -nz <num>                 : Set the default number of zones to activate\n"
		       "  -ur y|n                   : Enable of disable unrestricted reads\n"
		       "  -maxr <num>|\"unlimited\" : Set the maximum number of realms to activate\n\n",
		       argv[0]);
		return 1;
	}

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-v") == 0) {
			zbc_set_log_level("debug");
		} else if (strcmp(argv[i], "-scsi") == 0) {
			oflags = ZBC_O_DRV_SCSI;
		} else if (strcmp(argv[i], "-ata") == 0) {
			oflags = ZBC_O_DRV_ATA;
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
		} else if (strcmp(argv[i], "-maxr") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "unlimited") == 0) {
				max_activate = 0xfffe;
			} else {
				max_activate = strtol(argv[i], NULL, 10);
				if (max_activate <= 0) {
					fprintf(stderr, "invalid -maxr value\n");
					goto usage;
				}
			}
			set_max_activate = true;
		} else if (strcmp(argv[i], "-ur") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "y") == 0)
				urswrz = true;
			else if (strcmp(argv[i], "n") == 0)
				urswrz = false;
			else {
				fprintf(stderr, "-ur value must be y or n\n");
				goto usage;
			}
			set_urswrz = true;
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
	ret = zbc_open(path, oflags | O_RDONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "zbc_open(%s) failed %d %s\n",
				path, ret, strerror(-ret));
		return 1;
	}

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	if (!zbc_device_is_zdr(&info)) {
		if (set_nz || set_urswrz || set_max_activate) {
			fprintf(stderr, "Not a ZDR device\n");
			ret = 1;
		}
		goto out;
	}

	/* Query the device about persistent XMR settings */
	ret = zbc_zone_activation_ctl(dev, &ctl, false);
	if (ret != 0) {
		fprintf(stderr, "zbc_zone_activation_ctl get failed %d\n",
			ret);
		ret = 1;
		goto out;
	}

	if (set_nz) {
		ctl.zbt_nr_zones = nz;
		upd = true;
	}
	if (set_urswrz) {
		ctl.zbt_urswrz = urswrz ? 0x01 : 0x00;
		upd = true;
	}
	if (set_max_activate) {
		ctl.zbt_max_activate = max_activate;
		upd = true;
	}

	if (upd) {
		if (!set_nz)
			ctl.zbt_nr_zones = 0xffffffff;
		if (!set_urswrz)
			ctl.zbt_urswrz = 0xff;
		if (!set_max_activate)
			ctl.zbt_max_activate = 0xffff;

		/* Need to change some values, request the device to update */
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			fprintf(stderr, "zbc_zone_activation_ctl set failed %d\n",
				ret);
			ret = 1;
			goto out;
		}

		/* Read back all the persistent XMR settings */
		ret = zbc_zone_activation_ctl(dev, &ctl, false);
		if (ret != 0) {
			fprintf(stderr, "zbc_zone_activation_ctl get failed %d\n",
				ret);
			ret = 1;
			goto out;
		}
	}

	zbc_print_device_info(&info, stdout);

	zbc_print_zone_activation_settings(&ctl);

out:
	zbc_close(dev);

	return ret;
}

