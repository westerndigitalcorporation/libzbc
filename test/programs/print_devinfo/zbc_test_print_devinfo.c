/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Author: Masato Suzuki (masato.suzuki@wdc.com)
 *         Damien Le Moal (damien.lemoal@wdc.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libzbc/zbc.h"
#include "zbc_private.h"

/**
 * Get last zone information (start LBA and size)
 */
static int zbc_get_last_zone(struct zbc_device *dev, struct zbc_zone *z)
{
	unsigned int nr_zones;
	struct zbc_zone *zones;
	int ret;

	/* Get zone list */
	ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
	if (ret != 0) {
		fprintf(stderr,
			"[TEST][ERROR],zbc_list_zones failed %d\n",
			ret);
		return ret;
	}

	memcpy(z, &zones[nr_zones - 1], sizeof(struct zbc_zone));

	free(zones);

	return 0;
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev;
	struct zbc_zone last_zone;
	unsigned int oflags;
	int ret;

	/* Check command line */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dev>\n", argv[0]);
		return 1;
	}

	/* Open device */
	oflags = ZBC_O_DEVTEST;
	oflags |= ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE;
	if (!getenv("ZBC_TEST_FORCE_ATA"))
		oflags |= ZBC_O_DRV_SCSI;

	ret = zbc_open(argv[1], oflags | O_RDONLY, &dev);
	if (ret != 0) {
		fprintf(stderr, "[TEST][ERROR],open device failed %d\n",
			ret);
		printf("[TEST][ERROR][SENSE_KEY],open-device-failed\n");
		printf("[TEST][ERROR][ASC_ASCQ],open-device-failed\n");
		return 1;
	}

	zbc_get_device_info(dev, &info);

	ret = zbc_get_last_zone(dev, &last_zone);
	if (ret != 0) {
		ret = 1;
		goto out;
	}

	fprintf(stdout,
		"[TEST][INFO][DEVICE_MODEL],%s\n",
		zbc_device_model_str(info.zbd_model));

	fprintf(stdout,
		"[TEST][INFO][MAX_NUM_OF_OPEN_SWRZ],%d\n",
		info.zbd_max_nr_open_seq_req);

	fprintf(stdout,
		"[TEST][INFO][MAX_LBA],%llu\n",
		(unsigned long long)info.zbd_lblocks - 1);

	fprintf(stdout,
		"[TEST][INFO][URSWRZ],%x\n",
		info.zbd_flags);

	fprintf(stdout,
		"[TEST][INFO][LAST_ZONE_LBA],%llu\n",
		(unsigned long long)zbc_sect2lba(&info, zbc_zone_start(&last_zone)));

	fprintf(stdout,
		"[TEST][INFO][LAST_ZONE_SIZE],%llu\n",
		(unsigned long long)zbc_sect2lba(&info, zbc_zone_length(&last_zone)));

out:
	zbc_close(dev);

	return ret;
}

