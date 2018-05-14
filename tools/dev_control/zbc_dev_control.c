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

static void zbc_print_supported_mutations(struct zbc_supported_mutation *sm)
{
	printf("MT: %u, Opt: %u\n", sm->zbs_mt, sm->zbs_opt.nz);
}

static void zbc_print_zone_activation_settings(struct zbc_zp_dev_control *ctl)
{
	printf("FSNOZ: %u, URSWRZ: %s, MAX ACTIVATION: %u\n",
	       ctl->zbm_nr_zones, ctl->zbm_urswrz ? "Y" : "N", ctl->zbm_max_activate);
}

int main(int argc, char **argv)
{
	struct zbc_device *dev;
	struct zbc_device_info info;
	struct zbc_zp_dev_control ctl;
	enum zbc_mutation_target mt = ZBC_MT_UNKNOWN;
	union zbc_mutation_opt opt;
	struct zbc_supported_mutation *sm = NULL;
	unsigned int nr_sm_recs, nrecs;
	int i, ret = 1, nz = 0, max_activate = 0;
	bool upd = false, urswrz = false, set_nz = false;
	bool list_mu = false, set_urswrz = false, set_max_activate = false;
	char *path;

	/* Check command line */
	if (argc < 2) {
usage:
		printf("Usage: %s [options] <dev>\n"
		       "Options:\n"
		       "  -v                      : Verbose mode\n"
		       "  -lm                     : List mutations supported by the device\n"
		       "  -mu <type> <model>      : Mutate to the specified numeric type and model\n"
		       "  -mu <target>            : Mutate to the specified target\n\n"
		       "  -nz <num>               : Set the default number of zones to convert\n"
		       "  -ur y|n                 : Enable of disable unrestricted reads\n"
		       "  -maxd <num>|\"unlimited\" : Set the maximum number of domains to activate\n\n"
		       "Mutation targets:\n"
		       "  NON_ZONED               : A classic, not zoned, device\n"
		       "  HM_ZONED                : Host-managed SMR device, no CMR zones\n"
		       "  HM_ZONED_1PCNT_B        : Host-managed SMR device, 1%% CMR at bottom\n"
		       "  HM_ZONED_2PCNT_BT       : Host-managed SMR device, 2%% CMR at bottom, one CMR zone at top\n"
		       "  HA_ZONED                : Host-aware SMR device, no CMR zones\n"
		       "  HA_ZONED_1PCNT_B        : Host-aware SMR device, 1%% CMR at bottom\n"
		       "  HA_ZONED_2PCNT_BT       : Host-aware SMR device, 2%% CMR at bottom, one CMR zone at top\n"
		       "  ZONE_ACT                : DH-SMR device supporting Zone Activation"
		       " command set, conventional CMR zones, no CMR-only domains\n"
		       "  ZA_1CMR_BOT             : Same as ZONE_ACT, but the first conversion domain"
		       " is CMR-only\n"
		       "  ZA_1CMR_BOT_TOP         : Same as ZONE_ACT, but the first and last conversion"
		       " domains are CMR-only\n"
		       "  ZONE_ACT_WPC            : DH-SMR device supporting Zone Activation"
		       " command set, WPC CMR zones, no CMR-only domains\n"
		       "  ZA_BARE_BONE            : DH-SMR device supporting Zone Activation and minimal features\n"
		       "  ZA_STX                  : Same as ZONE_ACT, but no DOMAIN REPORT \n"
		       "  ZA_FAULTY               : Same as ZONE_ACT, several offline and read-only zones injected \n",
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
			if (isdigit(argv[i][0])) {
				mt = strtol(argv[i], NULL, 0);
				if (i >= (argc - 1))
					goto usage;
				i++;
				opt.nz = (enum zbc_mutation_opt_nz)strtol(argv[i], NULL, 0);
			} else if (strcmp(argv[i], "NON_ZONED") == 0) {
				mt = ZBC_MT_NON_ZONED;
				opt.nz = ZBC_MO_NZ_GENERIC;
			} else if (strcmp(argv[i], "HM_ZONED") == 0) {
				mt = ZBC_MT_HM_ZONED;
				opt.smr = ZBC_MO_SMR_NO_CMR;
			} else if (strcmp(argv[i], "HM_ZONED_1PCNT_B") == 0) {
				mt = ZBC_MT_HM_ZONED;
				opt.smr = ZBC_MO_SMR_1PCNT_B;
			} else if (strcmp(argv[i], "HM_ZONED_2PCNT_BT") == 0) {
				mt = ZBC_MT_HM_ZONED;
				opt.smr = ZBC_MO_SMR_2PCNT_BT;
			} else if (strcmp(argv[i], "HA_ZONED") == 0) {
				mt = ZBC_MT_HA_ZONED;
				opt.smr = ZBC_MO_SMR_NO_CMR;
			} else if (strcmp(argv[i], "HA_ZONED_1PCNT_B") == 0) {
				mt = ZBC_MT_HA_ZONED;
				opt.smr = ZBC_MO_SMR_1PCNT_B;
			} else if (strcmp(argv[i], "HA_ZONED_2PCNT_BT") == 0) {
				mt = ZBC_MT_HA_ZONED;
				opt.smr = ZBC_MO_SMR_2PCNT_BT;
			} else if (strcmp(argv[i], "ZONE_ACT") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_NO_CMR;
			} else if (strcmp(argv[i], "ZA_1CMR_BOT") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_1_CMR_BOT;
			} else if (strcmp(argv[i], "ZA_1CMR_BOT_TOP") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_1_CMR_BOT_TOP;
			} else if (strcmp(argv[i], "ZONE_ACT_WPC") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_WPC_NO_CMR;
			} else if (strcmp(argv[i], "ZA_BARE_BONE") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_BBONE;
			} else if (strcmp(argv[i], "ZA_STX") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_STX;
			} else if (strcmp(argv[i], "ZA_FAULTY") == 0) {
				mt = ZBC_MT_ZONE_ACT;
				opt.za = ZBC_MO_ZA_FAULTY;
			}
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
		} else if (strcmp(argv[i], "-maxd") == 0) {
			if (i >= (argc - 1))
				goto usage;
			i++;

			if (strcmp(argv[i], "unlimited") == 0) {
				max_activate = 0xfffe;
			} else {
				max_activate = strtol(argv[i], NULL, 10);
				if (max_activate <= 0) {
					fprintf(stderr, "invalid -maxd value\n");
					goto usage;
				}
			}
			set_max_activate = true;
		} else if (strcmp(argv[i], "-lm") == 0) {
			list_mu = true;
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
	ret = zbc_open(path, ZBC_O_DRV_MASK | O_RDONLY, &dev);
	if (ret != 0)
		return 1;

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	if (mt == ZBC_MT_UNKNOWN)
		zbc_print_device_info(&info, stdout);

	if (list_mu) {
		if (!(info.zbd_flags & ZBC_MUTATE_SUPPORT)) {
			fprintf(stderr, "Device doesn't support MUTATE\n");
			ret = 1;
			goto out;
		}

		ret = zbc_report_nr_rpt_mutations(dev, &nr_sm_recs);
		if (ret != 0) {
			fprintf(stderr, "zbc_report_nr_rpt_mutations failed %d\n",
				ret);
			ret = 1;
			goto out;
		}

		printf("    %u supported mutation%s\n",
		       nr_sm_recs, (nr_sm_recs > 1) ? "s" : "");

		if (!nrecs || nrecs > nr_sm_recs)
			nrecs = nr_sm_recs;
		if (!nrecs)
			goto skip_lm;

		/* Allocate the array of supported mutation types/options */
		sm = (struct zbc_supported_mutation *)calloc(nrecs,
						 sizeof(struct zbc_supported_mutation));
		if (!sm) {
			fprintf(stderr, "No memory\n");
			ret = 1;
			goto out;
		}

		/* Get the supported mutationss */
		ret = zbc_report_mutations(dev, sm, &nrecs);
		if (ret != 0) {
			fprintf(stderr, "zbc_domain_report failed %d\n", ret);
			ret = 1;
			goto out;
		}

		for (i = 0; i < (int)nrecs; i++)
			zbc_print_supported_mutations(&sm[i]);

skip_lm:
		if (sm)
			free(sm);
	}

	if (mt != ZBC_MT_UNKNOWN) {
		if (!(info.zbd_flags & ZBC_MUTATE_SUPPORT)) {
			fprintf(stderr, "Device doesn't support MUTATE\n");
			ret = 1;
			goto out;
		}

		/* Try to mutate the device */
		ret = zbc_mutate(dev, mt, opt);
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
		if (set_nz || set_urswrz || set_max_activate) {
			fprintf(stderr, "Not a Zone Activation device\n");
			ret = 1;
		} else if (mt != ZBC_MT_UNKNOWN) {
			zbc_print_device_info(&info, stdout);
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
	if (set_urswrz) {
		ctl.zbm_urswrz = urswrz ? 0x01 : 0x00;
		upd = true;
	}
	if (set_max_activate) {
		ctl.zbm_max_activate = max_activate;
		upd = true;
	}

	if (upd) {
		if (!set_nz)
			ctl.zbm_nr_zones = 0xffffffff;
		if (!set_urswrz)
			ctl.zbm_urswrz = 0xff;
		if (!set_max_activate)
			ctl.zbm_max_activate = 0xffff;

		/* Need to change some values, request the device to update */
		ret = zbc_zone_activation_ctl(dev, &ctl, true);
		if (ret != 0) {
			fprintf(stderr, "zbc_zone_activation_ctl set failed %d\n",
				ret);
			ret = 1;
			goto out;
		}

		/* Read back all the persistent DH-SMR settings */
		ret = zbc_zone_activation_ctl(dev, &ctl, false);
		if (ret != 0) {
			fprintf(stderr, "zbc_zone_activation_ctl get failed %d\n",
				ret);
			ret = 1;
			goto out;
		}
	}

	if (mt != ZBC_MT_UNKNOWN)
		zbc_print_device_info(&info, stdout);

	zbc_print_zone_activation_settings(&ctl);

out:
	zbc_close(dev);

	return ret;
}

