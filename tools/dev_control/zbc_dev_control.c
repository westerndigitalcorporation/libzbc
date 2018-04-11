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
	printf("FSONZ: %u, CMR WP Check: %s\n",
	       ctl->zbm_nr_zones, ctl->zbm_cmr_wp_check ? "Y" : "N");
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
	int i, ret = 1, nz = 0;
	bool upd = false, wp_check = false, set_nz = false;
	bool list_mu = false, set_wp_chk = false;
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
		       "  -wpc y|n                : Enable of disable CMR write pointer check\n\n"
		       "Mutation targets:\n"
		       "  NON_ZONED               : A classic, not zoned, device\n"
		       "  HM_ZONED                : Host-managed SMR device, no CMR zones\n"
		       "  HM_ZONED_1PCNT_B        : Host-managed SMR device, 1%% CMR at bottom\n"
		       "  HM_ZONED_2PCNT_BT       : Host-managed SMR device, 2%% CMR at bottom, one CMR zone at top\n"
		       "  HA_ZONED                : Host-aware SMR device, no CMR zones\n"
		       "  HA_ZONED_1PCNT_B        : Host-aware SMR device, 1%% CMR at bottom\n"
		       "  HA_ZONED_2PCNT_BT       : Host-aware SMR device, 2%% CMR at bottom, one CMR zone at top\n"
		       "  ZONE_ACT                : DH-SMR device supporting Zone Activation"
		       " command set, no CMR-only zones\n"
		       "  ZA_1CMR_BOT             : Same as ZONE_ACT, but the first conversion domain"
		       " is CMR-only\n"
		       "  ZA_1CMR_BOT_TOP         : Same as ZONE_ACT, but the first and last conversion"
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
		} else if (strcmp(argv[i], "-lm") == 0) {
			list_mu = true;
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

