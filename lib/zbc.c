// SPDX-License-Identifier: BSD-2-Clause
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Christoph Hellwig (hch@infradead.org)
 */
#include "zbc.h"

#include <string.h>
#include <limits.h>
#include <assert.h>

/*
 * Log level.
 */
int zbc_log_level = ZBC_LOG_WARNING;

/*
 * Backend drivers.
 */
static struct zbc_drv *zbc_drv[] = {
	&zbc_scsi_drv,
	&zbc_ata_drv,
	NULL
};

/**
 * Sense key strings.
 */
static struct zbc_sg_sk_s {
	enum zbc_sk	sk;
	const char	*sk_name;
} zbc_sg_sk_list[] = {
	{ ZBC_SK_NOT_READY,		"Not-ready"		},
	{ ZBC_SK_MEDIUM_ERROR,		"Medium-error"		},
	{ ZBC_SK_ILLEGAL_REQUEST,	"Illegal-request"	},
	{ ZBC_SK_DATA_PROTECT,		"Data-protect"		},
	{ ZBC_SK_HARDWARE_ERROR,	"Hardware-error"	},
	{ ZBC_SK_ABORTED_COMMAND,	"Aborted-command"	},
	{ ZBC_SK_MEDIUM_ERROR,		"Medium-error"		},
	{ 0,				NULL }
};

/**
 * Sense code qualifiers strings.
 */
static struct zbc_sg_asc_ascq_s {
	enum zbc_asc_ascq	asc_ascq;
	const char		*ascq_name;
} zbc_sg_asc_ascq_list[] = {

	{
		ZBC_ASC_INVALID_FIELD_IN_CDB,
		"Invalid-field-in-cdb"
	},
	{
		ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE,
		"Logical-block-address-out-of-range"
	},
	{
		ZBC_ASC_UNALIGNED_WRITE_COMMAND,
		"Unaligned-write-command"
	},
	{
		ZBC_ASC_WRITE_BOUNDARY_VIOLATION,
		"Write-boundary-violation"
	},
	{
		ZBC_ASC_ATTEMPT_TO_READ_INVALID_DATA,
		"Attempt-to-read-invalid-data"
	},
	{
		ZBC_ASC_READ_BOUNDARY_VIOLATION,
		"Read-boundary-violation"
	},
	{
		ZBC_ASC_ZONE_IS_READ_ONLY,
		"Zone-is-read-only"
	},
	{
		ZBC_ASC_ZONE_IS_OFFLINE,
		"Zone-is-offline"
	},
	{
		ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES,
		"Insufficient-zone-resources"
	},
	{
		ZBC_ASC_ZONE_IS_INACTIVE,
		"Zone-is-inactive"
	},
	{
		ZBC_ASC_ATTEMPT_TO_ACCESS_GAP_ZONE,
		"Attempt-to-access-GAP-zone"
	},
	{
		ZBC_ASC_READ_ERROR,
		"Read-error"
	},
	{
		ZBC_ASC_WRITE_ERROR,
		"Write-error"
	},
	{
		ZBC_ASC_FORMAT_IN_PROGRESS,
		"Format-in-progress"
	},
	{
		ZBC_ASC_INTERNAL_TARGET_FAILURE,
		"Internal-target-failure"
	},
	{
		ZBC_ASC_INVALID_COMMAND_OPERATION_CODE,
		"Invalid-command-operation-code"
	},
	{
		ZBC_ASC_INVALID_FIELD_IN_PARAMETER_LIST,
		"Invalid-field-in-parameter-list"
	},
	{
		ZBC_ASC_PARAMETER_LIST_LENGTH_ERROR,
		"Parameter-list-length-error"
	},
	{
		ZBC_ASC_ZONE_RESET_WP_RECOMMENDED,
		"Zone-reset-wp-recommended"
	},
	{
		0,
		NULL
	}
};

/**
 * Per-thread local zbc_errno handling.
 */
__thread struct zbc_err_ext zerrno;

/**
 * zbc_version - get the library version as a string
 */
char const *zbc_version(void)
{
	static char ver[8];

	snprintf(ver, sizeof(ver) - 1, "%s", PACKAGE_VERSION);
	return (char const*)ver;
}

/**
 * zbc_set_log_level - Set the library log level
 */
void zbc_set_log_level(char const *log_level)
{
	if (!log_level) {
		/* Set default */
		zbc_log_level = ZBC_LOG_ERROR;
		return;
	}

	if (strcmp(log_level, "none") == 0)
		zbc_log_level = ZBC_LOG_NONE;
	else if (strcmp(log_level, "warning") == 0)
		zbc_log_level = ZBC_LOG_WARNING;
	else if (strcmp(log_level, "error") == 0)
		zbc_log_level = ZBC_LOG_ERROR;
	else if (strcmp(log_level, "info") == 0)
		zbc_log_level = ZBC_LOG_INFO;
	else if (strcmp(log_level, "debug") == 0)
		zbc_log_level = ZBC_LOG_DEBUG;
	else
		fprintf(stderr, "Unknown log level \"%s\"\n",
			log_level);
}

/**
 * zbc_device_type_str - Returns a devicetype name
 */
const char *zbc_device_type_str(enum zbc_dev_type type)
{
	switch (type) {
	case ZBC_DT_SCSI:
		return "SCSI ZBC device";
	case ZBC_DT_ATA:
		return "ATA ZAC device";
	case ZBC_DT_UNKNOWN:
	default:
		return "Unknown-device-type";
	}
}

/**
 * zbc_device_model_str - Returns a device zone model name
 */
const char *zbc_device_model_str(enum zbc_dev_model model)
{
	switch (model) {
	case ZBC_DM_HOST_AWARE:
		return "Host-aware";
	case ZBC_DM_HOST_MANAGED:
		return "Host-managed";
	case ZBC_DM_DEVICE_MANAGED:
		return "Device-managed";
	case ZBC_DM_STANDARD:
		return "Standard block device";
	case ZBC_DM_DRIVE_UNKNOWN:
	default:
		return "Unknown-device-model";
	}
}

/**
 * zbc_zone_type_str - returns a string describing a zone type
 */
const char *zbc_zone_type_str(enum zbc_zone_type type)
{
	switch (type) {
	case ZBC_ZT_CONVENTIONAL:
		return "Conventional";
	case ZBC_ZT_SEQUENTIAL_REQ:
		return "Sequential-write-required";
	case ZBC_ZT_SEQUENTIAL_PREF:
		return "Sequential-write-preferred";
	case ZBC_ZT_SEQ_OR_BEF_REQ:
		return "Sequential-or-before-required";
	case ZBC_ZT_GAP:
		return "Gap";
	case ZBC_ZT_UNKNOWN:
	default:
		return "Unknown-zone-type";
	}
}

/**
 * zbc_zone_cond_str - Returns a string describing a zone condition
 */
const char *zbc_zone_condition_str(enum zbc_zone_condition cond)
{
	switch (cond) {
	case ZBC_ZC_NOT_WP:
		return "Not-write-pointer";
	case ZBC_ZC_EMPTY:
		return "Empty";
	case ZBC_ZC_IMP_OPEN:
		return "Implicit-open";
	case ZBC_ZC_EXP_OPEN:
		return "Explicit-open";
	case ZBC_ZC_CLOSED:
		return "Closed";
	case ZBC_ZC_INACTIVE:
		return "Inactive";
	case ZBC_ZC_RDONLY:
		return "Read-only";
	case ZBC_ZC_FULL:
		return "Full";
	case ZBC_ZC_OFFLINE:
		return "Offline";
	default:
		return "Unknown-zone-condition";
	}
}

/* zbc_errno_ext - Get detailed error code of last operation
 *
 * If size >= sizeof(struct zbc_err) then return the entire zbc_err structure;
 * otherwise return the first size bytes of the structure.
 */

void zbc_errno_ext(struct zbc_device *dev, struct zbc_err_ext *err, size_t size)
{
	if (size > sizeof(struct zbc_err_ext))
		size = sizeof(struct zbc_err_ext);

	memcpy(err, &zerrno, size);
}

/**
 * zbc_errno - Get detailed error code of last operation
 */
void zbc_errno(struct zbc_device *dev, struct zbc_errno *err)
{
	zbc_errno_ext(dev, (struct zbc_err_ext *)err, sizeof(struct zbc_errno));
}

/**
 * zbc_sk_str - Returns a string describing a sense key
 */
const char *zbc_sk_str(enum zbc_sk sk)
{
	static char sk_buf[64];
	int i = 0;

	while (zbc_sg_sk_list[i].sk != 0) {
		if (sk == zbc_sg_sk_list[i].sk)
			return zbc_sg_sk_list[i].sk_name;
		i++;
	}

	sprintf(sk_buf, "Unknown-sense-key 0x%02X", (int)sk);

	return sk_buf;
}

/**
 * zbc_asc_ascq_str - Returns a string describing a sense code
 */
const char *zbc_asc_ascq_str(enum zbc_asc_ascq asc_ascq)
{
	static char asc_buf[64];
	int i = 0;

	while (zbc_sg_asc_ascq_list[i].asc_ascq != 0) {
		if (asc_ascq == zbc_sg_asc_ascq_list[i].asc_ascq)
			return zbc_sg_asc_ascq_list[i].ascq_name;
		i++;
	}

	sprintf(asc_buf,
		"Unknown-additional-sense-code-qualifier 0x%02X",
		(int)asc_ascq);

	return asc_buf;
}

static int zbc_realpath(const char *filename, char **path)
{
	char *p;
	int ret = 0;

	/* Follow symlinks (required for device mapped devices) */
	p = realpath(filename, NULL);
	if (p) {
		*path = p;
	} else {
		ret = -errno;
		zbc_error("%s: Failed to get real path %d (%s)\n",
			  filename,
			  errno, strerror(errno));
	}

	return ret;
}

/**
 * Get additional domain info for ZR devices.
 */
static int zbc_get_domain_info(struct zbc_device *dev)
{
	struct zbc_device_info *di = &dev->zbd_info;
	struct zbc_zone_domain *domains, *d;
	unsigned int nr_domains;
	int i, ret;
	unsigned short dom_flgs;

	if (di->zbd_flags & ZBC_ZONE_REALMS_SUPPORT) {
		/*
		 * Issue REPORT DOMAINS to fetch domain flags and check
		 * if any domains have shifting realm boundaries.
		 */
		ret = zbc_list_domains(dev, 0LL, ZBC_RZD_RO_ALL, &domains, &nr_domains);
		if (ret < 0)
			return ret;

		for (i = 0, d = domains; i < (int)nr_domains; i++, d++) {
			dom_flgs = zbc_zone_domain_flags(d);
			if ((dom_flgs & ZBC_ZDF_VALID_ZONE_TYPE) &&
			    (dom_flgs & ZBC_ZDF_SHIFTING_BOUNDARIES)) {
				switch (zbc_zone_domain_type(d)) {
				case ZBC_ZT_CONVENTIONAL:
					di->zbd_flags |= ZBC_CONV_REALMS_SHIFTING;
					break;
				case ZBC_ZT_SEQUENTIAL_REQ:
					di->zbd_flags |= ZBC_SEQ_REQ_REALMS_SHIFTING;
					break;
				case ZBC_ZT_SEQUENTIAL_PREF:
					di->zbd_flags |= ZBC_SEQ_PREF_REALMS_SHIFTING;
					break;
				case ZBC_ZT_SEQ_OR_BEF_REQ:
					di->zbd_flags |= ZBC_SOBR_REALMS_SHIFTING;
					break;
				default:;
				}
			}
		}

		free(domains);
	}

	return 0;
}

/**
 * zbc_device_is_zoned - Test if a physical device is zoned.
 */
int zbc_device_is_zoned(const char *filename, bool unused,
			struct zbc_device_info *info)
{
	struct zbc_device *dev = NULL;
	char *path = NULL;
	int ret, i;

	ret = zbc_realpath(filename, &path);
	if (ret)
		return ret;

	/* Test all backends until one accepts the drive. */
	for (i = 0; zbc_drv[i]; i++) {
		ret = zbc_drv[i]->zbd_open(path, O_RDONLY, &dev);
		if (ret == 0) {
			/* This backend accepted the device */
			dev->zbd_drv = zbc_drv[i];
			break;
		}
		if (ret != -ENXIO)
			goto out;
	}

	if (dev && dev->zbd_drv) {
		if (zbc_get_domain_info(dev) < 0) {
			ret = 0;
			goto out;
		}

		ret = 1;
		if (info)
			memcpy(info, &dev->zbd_info,
			       sizeof(struct zbc_device_info));
		dev->zbd_drv->zbd_close(dev);
	} else {
		if ((ret != -EPERM) && (ret != -EACCES))
			ret = 0;
	}

out:
	free(path);

	return ret;
}

/**
 * zbc_open - open a ZBC device
 */
int zbc_open(const char *filename, int flags, struct zbc_device **pdev)
{
	struct zbc_device *dev = NULL;
	unsigned int allowed_drv;
	char *path = NULL;
	int ret, i, mask = ZBC_O_DRV_MASK;

	ret = zbc_realpath(filename, &path);
	if (ret)
		return ret;

#ifdef HAVE_DEVTEST
	if (flags & ZBC_O_DEVTEST)
		mask = ZBC_O_TEST_DRV_MASK;
#endif

	allowed_drv = flags & mask;
	if (!allowed_drv)
		allowed_drv = mask;

	/* Test all backends until one accepts the drive */
	ret = -ENODEV;
	for (i = 0; zbc_drv[i] != NULL; i++) {

		if (!(zbc_drv[i]->flag & allowed_drv))
			continue;

		ret = zbc_drv[i]->zbd_open(path, flags, &dev);
		switch (ret) {
		case 0:
			/* This backend accepted the drive */
			dev->zbd_drv = zbc_drv[i];
			*pdev = dev;
			goto out;
		case -ENXIO:
			continue;
		default:
			goto out;
		}

	}

out:
	if (!ret)
		ret = zbc_get_domain_info(dev);

	free(path);
	return ret;
}

/**
 * zbc_close - close a ZBC Device
 */
int zbc_close(struct zbc_device *dev)
{
	return dev->zbd_drv->zbd_close(dev);
}

/**
 * zbc_get_device_info - Get a ZBC device information
 */
void zbc_get_device_info(struct zbc_device *dev, struct zbc_device_info *info)
{
	memcpy(info, &dev->zbd_info, sizeof(struct zbc_device_info));
}

/**
 * zbc_print_device_info - Print a device information
 */
void zbc_print_device_info(struct zbc_device_info *info, FILE *out)
{
	char tmp[64];

	fprintf(out,
		"    Vendor ID: %s\n",
		info->zbd_vendor_id);
	if (info->zbd_model == ZBC_DM_STANDARD) {
		fprintf(out,
			"    %s interface, standard block device\n",
			zbc_device_type_str(info->zbd_type));
	} else {
		fprintf(out,
			"    %s interface, %s zone model\n",
			zbc_device_type_str(info->zbd_type),
			zbc_device_model_str(info->zbd_model));
	}
	fprintf(out,
		"    %llu 512-bytes sectors\n",
		(unsigned long long) info->zbd_sectors);
	fprintf(out,
		"    %llu logical blocks of %u B\n",
		(unsigned long long) info->zbd_lblocks,
		(unsigned int) info->zbd_lblock_size);
	fprintf(out,
		"    %llu physical blocks of %u B\n",
		(unsigned long long) info->zbd_pblocks,
		(unsigned int) info->zbd_pblock_size);
	fprintf(out,
		"    %.03F GB capacity\n",
		(double)(info->zbd_sectors << 9) / 1000000000);

	if (info->zbd_model == ZBC_DM_HOST_MANAGED ||
	    info->zbd_model == ZBC_DM_HOST_AWARE)
		fprintf(out,
			"    Read commands are %s\n",
			(info->zbd_flags & ZBC_UNRESTRICTED_READ) ?
			"unrestricted" : "restricted");

	fprintf(out,
		"    %llu KiB max R/W size\n",
		(unsigned long long)(info->zbd_max_rw_sectors << 9) / 1024);

	if (info->zbd_model == ZBC_DM_HOST_MANAGED) {

		if (info->zbd_max_nr_open_seq_req == ZBC_NO_LIMIT)
			strcpy(tmp, "unlimited");
		else
			sprintf(tmp, "%u",
				(unsigned int) info->zbd_max_nr_open_seq_req);
		fprintf(out,
			"    Maximum number of open sequential write "
			"required zones: %s\n", tmp);

	} else if (info->zbd_model == ZBC_DM_HOST_AWARE) {

		if (info->zbd_opt_nr_open_seq_pref == ZBC_NOT_REPORTED)
			strcpy(tmp, "not reported");
		else
			sprintf(tmp, "%u",
				(unsigned int)info->zbd_opt_nr_open_seq_pref);
		fprintf(out,
			"    Optimal number of open sequential write "
			"preferred zones: %s\n", tmp);

		if (info->zbd_opt_nr_non_seq_write_seq_pref == ZBC_NOT_REPORTED)
			strcpy(tmp, "not reported");
		else
			sprintf(tmp, "%u",
				(unsigned int)info->zbd_opt_nr_non_seq_write_seq_pref);

		fprintf(out,
			"    Optimal number of non-sequentially written "
			"sequential write preferred zones: %s\n", tmp);

	}

	if (info->zbd_model != ZBC_DM_STANDARD) {
		fprintf(out, "    Zone Domains command set is %ssupported\n",
			(info->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    Zone Realms command set is %ssupported\n",
			(info->zbd_flags & ZBC_ZONE_REALMS_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    Zone operation counts are %ssupported\n",
			zbc_zone_count_supported(info) ? "" : "NOT ");
	}
	if ((info->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) ||
	    (info->zbd_flags & ZBC_ZONE_REALMS_SUPPORT)) {
		fprintf(out, "    Unrestricted read control is %ssupported\n",
			(info->zbd_flags & ZBC_URSWRZ_SET_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    Zone Activation control is %ssupported\n",
			(info->zbd_flags & ZBC_ZA_CONTROL_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    NOZSRC bit is %ssupported\n",
			(info->zbd_flags & ZBC_NOZSRC_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    REPORT REALMS command is %ssupported\n",
			(info->zbd_flags & ZBC_REPORT_REALMS_SUPPORT) ? "" : "NOT ");
		if (info->zbd_flags & ZBC_MAXACT_SET_SUPPORT) {
			fprintf(out,
				"    Setting maximum number of zones "
				"to activate is supported\n");
		}
		if (info->zbd_max_activation != 0)
			fprintf(out, "    Maximum number of zones to activate: %u\n",
				info->zbd_max_activation);
		else
			fprintf(out, "    Maximum number of zones"
				     " to activate is unlimited\n");
		fprintf(out, "    Supported zone types: %s%s%s%s%s\n",
			(info->zbd_flags & ZBC_CONV_ZONE_SUPPORT) ? "Conv " : "",
			(info->zbd_flags & ZBC_SEQ_REQ_ZONE_SUPPORT) ? "SWR " : "",
			(info->zbd_flags & ZBC_SEQ_PREF_ZONE_SUPPORT) ? "SWP " : "",
			(info->zbd_flags & ZBC_SOBR_ZONE_SUPPORT) ? "SOBR " : "",
			(info->zbd_flags & ZBC_GAP_ZONE_SUPPORT) ? "GAP " : "");
	}
	if (info->zbd_flags & ZBC_ZONE_REALMS_SUPPORT) {
		if (info->zbd_flags & ZBC_CONV_REALMS_SHIFTING)
			fprintf(out, "    Realms of Conventional zones have shifting boundaries\n");
		if (info->zbd_flags & ZBC_SEQ_REQ_REALMS_SHIFTING)
			fprintf(out, "    Realms of SWR zones have shifting boundaries\n");
		if (info->zbd_flags & ZBC_SEQ_PREF_REALMS_SHIFTING)
			fprintf(out, "    Realms of SWP zones have shifting boundaries\n");
		if (info->zbd_flags & ZBC_SOBR_REALMS_SHIFTING)
			fprintf(out, "    Realms of SOBR zones have shifting boundaries\n");
	}

	fflush(out);
}

/**
 * zbc_report_zones - Get zone information
 */
int zbc_report_zones(struct zbc_device *dev, uint64_t sector,
		     enum zbc_zone_reporting_options ro,
		     struct zbc_zone *zones, unsigned int *nr_zones)
{
	unsigned int max_zones = *nr_zones;
	unsigned int n, nz = 0;
	uint64_t last_sector;
	int ret;

	if (!zbc_test_mode(dev) && sector >= dev->zbd_info.zbd_sectors) {
		/* No zones to report beyond drive capacity */
		*nr_zones = 0;
		return 0;
	}

	if (!zones)
		return (dev->zbd_drv->zbd_report_zones)(dev, sector,
							zbc_rz_ro_mask(ro),
							NULL, nr_zones);

	/* Get zone information */
	while (nz < max_zones &&
		sector < dev->zbd_info.zbd_sectors) {

		n = max_zones - nz;
		ret = (dev->zbd_drv->zbd_report_zones)(dev, sector,
					zbc_rz_ro_mask(ro) | ZBC_RO_PARTIAL,
					&zones[nz], &n);
		if (ret != 0) {
			zbc_error("%s: Get zones from sector %llu failed %d (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long) sector,
				  ret, strerror(-ret));
			break;
		}

		if (n == 0)
			break;

		nz += n;

		last_sector = zones[nz - 1].zbz_start + zones[nz - 1].zbz_length;
		if (last_sector >= dev->zbd_info.zbd_sectors)
			break;
		sector = last_sector;
	}

	if (!ret)
		*nr_zones = nz;

	return ret;
}

/**
 * zbc_list_zones - Get zone information
 */
int zbc_list_zones(struct zbc_device *dev, uint64_t sector,
		   enum zbc_zone_reporting_options ro,
		   struct zbc_zone **pzones, unsigned int *pnr_zones)
{
	struct zbc_zone *zones = NULL;
	unsigned int nr_zones = 0;
	int ret;

	/* Get total number of zones */
	ret = zbc_report_nr_zones(dev, sector, zbc_rz_ro_mask(ro), &nr_zones);
	if (ret < 0)
		return ret;

	if (!nr_zones)
		goto out;

	zbc_debug("%s: %d zones\n",
		  dev->zbd_filename,
		  nr_zones);

	/* Allocate zone array */
	zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));
	if (!zones)
		return -ENOMEM;

	/* Get zone information */
	ret = zbc_report_zones(dev, sector, zbc_rz_ro_mask(ro), zones, &nr_zones);
	if (ret != 0) {
		zbc_error("%s: zbc_report_zones failed %d\n",
			  dev->zbd_filename, ret);
		free(zones);
		return ret;
	}

out:
	*pzones = zones;
	*pnr_zones = nr_zones;

	return 0;
}

/**
 * zbc_zone_group_op - Execute an operation on a group of zones
 */
int zbc_zone_group_op(struct zbc_device *dev, uint64_t sector,
		      unsigned int count, enum zbc_zone_op op,
		      unsigned int flags)
{
	struct zbc_zone zone;
	unsigned int i, nr_zones;
	int ret;

	if (!zbc_test_mode(dev) &&
	    (!(flags & ZBC_OP_ALL_ZONES)) &&
	    !zbc_dev_sect_laligned(dev, sector))
		return -EINVAL;

	if (count <= 1 || (flags & ZBC_OP_ALL_ZONES))
		return (dev->zbd_drv->zbd_zone_op)(dev, sector, 0, op, flags);

	if (zbc_zone_count_supported(&dev->zbd_info)) {
		return (dev->zbd_drv->zbd_zone_op)(dev, sector, count, op, flags);
	} else {
		zbc_debug("%s: zone op COUNT is not supported by drive, emulating\n",
			    dev->zbd_filename);
		nr_zones = 1;
		ret = zbc_report_zones(dev, sector, 0, &zone, &nr_zones);
		if (ret)
			return ret;

		for (i = 0; i < count; i++) {
			ret = (dev->zbd_drv->zbd_zone_op)(dev, sector, 0, op, flags);
			if (ret)
				return ret;
			sector += zone.zbz_length;
		}

		return 0;
	}
}

/**
 * zbc_zone_operation - Execute an operation on a zone
 */
int zbc_zone_operation(struct zbc_device *dev, uint64_t sector,
		       enum zbc_zone_op op, unsigned int flags)
{
	return zbc_zone_group_op(dev, sector, 0, op, flags);
}

/**
 * zbc_report_domains - Get zone domain information
 */
int zbc_report_domains(struct zbc_device *dev, uint64_t sector,
		       enum zbc_domain_report_options ro,
		       struct zbc_zone_domain *domains,
		       unsigned int nr_domains)
{
	int ret;

	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	if (!dev->zbd_drv->zbd_report_domains) {
		/* FIXME need to implement! */
		zbc_warning("%s: REPORT DOMAINS not implemented by driver\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	ret = (dev->zbd_drv->zbd_report_domains)(dev, sector, ro, domains, nr_domains);
	if (ret < 0) {
		zbc_error("%s: REPORT DOMAINS failed %d (%s)\n",
			  dev->zbd_filename,
			  ret, strerror(-ret));
	}

	return ret;
}

#define ZBC_EST_ALLOC_DOMAINS	6

/**
 * zbc_list_domains - Allocate a buffer and fill it with zone
 * 		      domain information
 */
int zbc_list_domains(struct zbc_device *dev, uint64_t sector,
		     enum zbc_domain_report_options ro,
		     struct zbc_zone_domain **pdomains,
		     unsigned int *pnr_domains)
{
	struct zbc_zone_domain *domains = NULL;
	unsigned int nr_domains;
	int ret;

	if (!pdomains) {
		zbc_error("%s: NULL domain array pointer\n",
			  dev->zbd_filename);
		return -EINVAL;
	}
	*pdomains = NULL;
	if (!pnr_domains) {
		zbc_error("%s: NULL domain count pointer\n",
			  dev->zbd_filename);
		return -EINVAL;
	}
	*pnr_domains = 0;

	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	/*
	 * The number of zone domains is usually small, try allocating
	 * the buffer to hold a few domains and see if it will be enough.
	 * This will likely save us a SCSI call.
	 */
	domains = (struct zbc_zone_domain *)calloc(ZBC_EST_ALLOC_DOMAINS,
					 sizeof(struct zbc_zone_domain));
	if (!domains)
		return -ENOMEM;

	/* Get zone domain information */
	ret = zbc_report_domains(dev, sector, ro, domains,
				 ZBC_EST_ALLOC_DOMAINS);
	if (ret < 0) {
		zbc_error("%s: zbc_report_domains failed %d\n",
			  dev->zbd_filename, ret);
		free(domains);
		return ret;
	}
	nr_domains = ret;

	if (nr_domains > ZBC_EST_ALLOC_DOMAINS) {
		/* Reallocate zone domain descriptor array */
		free(domains);
		domains = (struct zbc_zone_domain *)calloc(nr_domains,
					 sizeof(struct zbc_zone_domain));
		if (!domains)
			return -ENOMEM;

		/* Get zone domain information again */
		ret = zbc_report_domains(dev, sector, ro, domains, nr_domains);
		if (ret < 0) {
			zbc_error("%s: zbc_report_domains failed %d\n",
				  dev->zbd_filename, ret);
			free(domains);
			return ret;
		}
		nr_domains = ret;
	}

	*pdomains = domains;
	*pnr_domains = nr_domains;

	return 0;
}

/**
 * zbc_zone_domain_start_lba - Get zone domain start logical block
 */
uint64_t zbc_zone_domain_start_lba(struct zbc_device *dev,
				   struct zbc_zone_domain *d)
{
	return zbc_dev_sect2lba(dev, d->zbm_start_sector);
}

/**
 * zbc_zone_domain_end_lba - Get zone domain end logical block
 */
uint64_t zbc_zone_domain_end_lba(struct zbc_device *dev,
				 struct zbc_zone_domain *d)
{
	return zbc_dev_sect2lba(dev, d->zbm_end_sector);
}

/** @brief Get zone domain end 512B sector */
uint64_t zbc_zone_domain_high_sect(struct zbc_device *dev,
				  struct zbc_zone_domain *d)
{
	return zbc_dev_lba2sect(dev, zbc_zone_domain_end_lba(dev, d) + 1) - 1;
}

/**
 * zbc_report_realms - Get zone realm information
 */
int zbc_report_realms(struct zbc_device *dev, uint64_t sector,
		      enum zbc_realm_report_options ro,
		      struct zbc_zone_realm *realms, unsigned int *nr_realms)
{
	struct zbc_device_info *di = &dev->zbd_info;
	int ret;

	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	/* If realm array is not provided, just get the number of realms */
	if (!realms)
		*nr_realms = 0;

	/* Get zone realm information */
	if (di->zbd_flags & ZBC_REPORT_REALMS_SUPPORT) {
		ret = (dev->zbd_drv->zbd_report_realms)(dev, sector, ro,
							realms, nr_realms);
		if (ret != 0) {
			zbc_error("%s: REPORT REALMS failed %d (%s)\n",
				  dev->zbd_filename,
				  ret, strerror(-ret));
		}
	} else {
		zbc_error("%s: REPORT REALMS is not supported by device\n",
			  dev->zbd_filename);
		ret = -ENXIO;
	}

	return ret;
}

/**
 * zbc_list_zone_realms - List zone realm information
 */
int zbc_list_zone_realms(struct zbc_device *dev, uint64_t sector,
			 enum zbc_realm_report_options ro,
			 struct zbc_zone_realm **prealms,
			 unsigned int *pnr_realms)
{
	struct zbc_zone_realm *realms = NULL;
	unsigned int nr_realms;
	int ret;

	if (!prealms) {
		zbc_error("%s: NULL realm array pointer\n",
			  dev->zbd_filename);
		return -EINVAL;
	}
	*prealms = NULL;

	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	/* Get the total number of realm descriptors */
	ret = zbc_report_nr_realms(dev, &nr_realms);
	if (ret < 0)
		return ret;

	zbc_debug("%s: %d zone realms\n",
		  dev->zbd_filename, nr_realms);

	/* Allocate zone realm descriptor array */
	realms = (struct zbc_zone_realm *)calloc(nr_realms,
						 sizeof(struct zbc_zone_realm));
	if (!realms)
		return -ENOMEM;

	/* Get zone realm information */
	ret = zbc_report_realms(dev, sector, ro, realms, &nr_realms);
	if (ret != 0) {
		zbc_error("%s: zbc_report_realms failed %d\n",
			  dev->zbd_filename, ret);
		free(realms);
		return ret;
	}

	*prealms = realms;
	*pnr_realms = nr_realms;

	return 0;
}

/**
 * zbc_realm_start_lba - Get the starting LBA of a realm
 *			 in a particular domain.
 */
uint64_t zbc_realm_start_lba(struct zbc_device *dev, struct zbc_zone_realm *r,
			     unsigned int dom_id)
{
	return zbc_dev_sect2lba(dev, zbc_realm_start_sector(r, dom_id));
}

/**
 * zbc_realm_end_lba - Get the ending LBA of a realm in a particular domain.
 */
uint64_t zbc_realm_end_lba(struct zbc_device *dev, struct zbc_zone_realm *r,
			   unsigned int dom_id)
{
	return zbc_dev_sect2lba(dev, zbc_realm_end_sector(r, dom_id));
}

/**
 * zbc_realm_high_sector - Get the highest sector
 *                         of a realm in a particular domain.
 */
uint64_t zbc_realm_high_sector(struct zbc_device *dev,
			       struct zbc_zone_realm *r, unsigned int dom_id)
{
	return zbc_dev_lba2sect(dev,
				zbc_realm_end_lba(dev, r, dom_id) + 1) - 1;
}

/**
 * zbc_zone_activate - Activate all zones in a specified range to a new type
 */
int zbc_zone_activate(struct zbc_device *dev, bool zsrc,
		      bool all, bool use_32_byte_cdb, uint64_t sector,
		      unsigned int nr_zones, unsigned int domain_id,
		      struct zbc_actv_res *actv_recs,
		      unsigned int *nr_actv_recs)
{
	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}
	if (!dev->zbd_drv->zbd_zone_query_actv) {
		/* FIXME need to implement! */
		zbc_warning("%s: Zone activate/query is not implemented\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	/* Execute the operation */
	return (dev->zbd_drv->zbd_zone_query_actv)(dev, zsrc,
						   all, use_32_byte_cdb,
						   false, sector, nr_zones,
						   domain_id, actv_recs,
						   nr_actv_recs);
}

/**
 * zbc_zone_query - Receive information about activating all zones in a
 *                  specific range to a new domain without actually performing
 *                  the activation process
 */
int zbc_zone_query(struct zbc_device *dev, bool zsrc,
		   bool all, bool use_32_byte_cdb, uint64_t sector,
		   unsigned int nr_zones, unsigned int domain_id,
		   struct zbc_actv_res *actv_recs, unsigned int *nr_actv_recs)
{
	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}
	if (!dev->zbd_drv->zbd_zone_query_actv) {
		/* FIXME need to implement! */
		zbc_warning("%s: Zone activation/query is not implemented\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	/* Execute the operation */
	return (dev->zbd_drv->zbd_zone_query_actv)(dev, zsrc,
						   all, use_32_byte_cdb,
						   true, sector, nr_zones,
						   domain_id, actv_recs,
						   nr_actv_recs);
}

/**
 * zbc_get_nr_actv_records - Get the expected number of activation records
 * 			     for a ZONE ACTIVATE or ZONE QUERY operation
 */
int zbc_get_nr_actv_records(struct zbc_device *dev, bool zsrc, bool all,
			    bool use_32_byte_cdb, uint64_t sector,
			    unsigned int nr_zones, unsigned int domain_id)
{
	uint32_t nr_actv_recs = 0;
	int ret;

	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}
	if (!dev->zbd_drv->zbd_zone_query_actv) {
		/* FIXME need to implement! */
		zbc_warning("%s: Zone activation/query is not implemented\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	ret = (dev->zbd_drv->zbd_zone_query_actv)(dev, zsrc,
						  all, use_32_byte_cdb,
						  true, sector, nr_zones,
						  domain_id, NULL,
						  &nr_actv_recs);
	return ret ? ret : (int)nr_actv_recs;
}

/**
 * zbc_zone_query_list - Receive information about activating all zones in a
 *                       specific range in a new domain without actually
 *                       performing the activation process. This function is
 *                       similar to \a zbc_zone_query, but it will allocate
 *                       the buffer space for the output list of activation
 *                       results. It is responsibility of the caller to free
 *                       this buffer
 */
int zbc_zone_query_list(struct zbc_device *dev, bool zsrc, bool all,
			bool use_32_byte_cdb, uint64_t sector,
			unsigned int nr_zones, unsigned int domain_id,
			struct zbc_actv_res **pactv_recs,
			unsigned int *pnr_actv_recs)
{
	struct zbc_actv_res *actv_recs = NULL;
	uint32_t nr_actv_recs;
	int ret;

	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	ret = zbc_get_nr_actv_records(dev, zsrc, all, use_32_byte_cdb,
				      sector, nr_zones, domain_id);
	if (ret < 0)
		return ret;
	nr_actv_recs = (uint32_t)ret;

	/* Allocate activation results record array */
	actv_recs = (struct zbc_actv_res *)
		    calloc(nr_actv_recs, sizeof(struct zbc_actv_res));
	if (!actv_recs)
		return -ENOMEM;

	/* Now get the entire list */
	ret = (dev->zbd_drv->zbd_zone_query_actv)(dev, zsrc,
						  all, use_32_byte_cdb,
						  true, sector, nr_zones,
						  domain_id, actv_recs,
						  &nr_actv_recs);
	*pactv_recs = actv_recs;
	*pnr_actv_recs = nr_actv_recs;

	return ret;
}

/**
 * zbc_zone_activation_ctl - Get or set device XMR configuration parameters
 */
int zbc_zone_activation_ctl(struct zbc_device *dev,
			    struct zbc_zd_dev_control *ctl, bool set)
{
	if (!zbc_dev_is_zdr(dev)) {
		zbc_error("%s: Not a ZD/ZR device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	if (!dev->zbd_drv->zbd_dev_control) {
		/* FIXME need to implement! */
		zbc_warning("%s: XMR dev_ctl not implemented by driver\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	return (dev->zbd_drv->zbd_dev_control)(dev, ctl, set);
}

/**
 * Convert vector buffer sizes to bytes for the vector
 * range between @sector_offset and @sector_offset + @sectors.
 * Limit the total size of the converted vector to the maximum allowed
 * number of pages to ensure that it can be mapped in the kernel for the
 * execution of the IO using it.
 * Return the number of buffers in the converted buffers. Set the size of
 * the converted vector buffer to @sectors.
 */
static int zbc_iov_convert(struct iovec *_iov, const struct iovec *iov,
			   int iovcnt, size_t sector_offset, size_t *sectors,
			   size_t max_sectors)
{
	unsigned int max_pages = (max_sectors << 9) / PAGE_SIZE;
	unsigned int np, nr_pages = 0;
	unsigned long base_offset;
	size_t sz, size = *sectors << 9;
	size_t offset = sector_offset << 9;
	size_t length, count = 0;
        int i, j = 0;

	for (i = 0; i < iovcnt && count < size && nr_pages < max_pages; i++) {

		length = iov[i].iov_len << 9;
		if (offset >= length) {
			offset -= length;
			continue;
		}

		_iov[j].iov_base = iov[i].iov_base + offset;
		length -= offset;
		offset = 0;

		if (count + length > size)
			length = size - count;

		/*
		 * Check page alignment of buffer start and end to get required
		 * number of pages.
		 */
		np = (length + PAGE_MASK) / PAGE_SIZE;
		base_offset = (unsigned long)_iov[j].iov_base & PAGE_MASK;
		if (base_offset)
			np++;

		/*
		 * If the number of pages exceeds the maximum, reduce
		 * the vector length.
		 */
		if (nr_pages + np > max_pages) {
			np = max_pages - nr_pages;
			if (base_offset)
				sz = (np - 1) * PAGE_SIZE;
			else
				sz = np * PAGE_SIZE;
			if (length > sz)
				length = sz;
		}

		_iov[j].iov_len = length;
		count += length;
		nr_pages += np;
		j++;

	}

	*sectors = count >> 9;

	return j;
}

/**
 * zbc_do_preadv - Execute a vector read
 */
static ssize_t zbc_do_preadv(struct zbc_device *dev,
			     const struct iovec *iov, int iovcnt,
			     uint64_t offset)
{
	size_t max_count = dev->zbd_info.zbd_max_rw_sectors;
	size_t count = zbc_iov_count(iov, iovcnt);
	struct iovec rd_iov[iovcnt];
	size_t rd_iov_count = 0, rd_iov_offset = 0;
	int rd_iovcnt;
	ssize_t ret;

	if (count << 9 > SSIZE_MAX)
		return -EINVAL;

	if (zbc_test_mode(dev)) {
		if (!count) {
			zbc_error("%s: zero-length read at sector %llu\n",
				  dev->zbd_filename,
				  (unsigned long long) offset);
			return -EINVAL;
		}
	} else {
		if (!zbc_dev_sect_laligned(dev, count) ||
		    !zbc_dev_sect_laligned(dev, offset)) {
			zbc_error("%s: Unaligned read %zu sectors at "
				  "sector %llu\n",
				  dev->zbd_filename,
				  count, (unsigned long long) offset);
			return -EIO;
		}

		if ((offset + count) > dev->zbd_info.zbd_sectors)
			count = dev->zbd_info.zbd_sectors - offset;
		if (!count || offset >= dev->zbd_info.zbd_sectors)
			return 0;
	}

	zbc_debug("%s: Read %zu sectors at sector %llu, %d vectors\n",
		  dev->zbd_filename,
		  count, (unsigned long long) offset, iovcnt);

	if (zbc_test_mode(dev) && count == 0) {
		zbc_iov_convert(rd_iov, iov, iovcnt, 0, &count, max_count);
		ret = (dev->zbd_drv->zbd_preadv)(dev, rd_iov, iovcnt, offset);
		if (ret < 0) {
			zbc_error("%s: read of zero sectors at sector %llu "
				  "failed %ld (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long) offset,
				  -ret, strerror(-ret));
		}
		return ret;
	}

	while (rd_iov_offset < count) {

		rd_iov_count = count - rd_iov_offset;
		rd_iovcnt = zbc_iov_convert(rd_iov, iov, iovcnt,
					    rd_iov_offset, &rd_iov_count,
					    max_count);

		ret = (dev->zbd_drv->zbd_preadv)(dev, rd_iov, rd_iovcnt,
						 offset);
		if (ret <= 0) {
			zbc_error("%s: Read %zu sectors at sector %llu "
				  "failed %zd (%s)\n",
				  dev->zbd_filename,
				  rd_iov_count, (unsigned long long) offset,
				  -ret, strerror(-ret));
			return ret ? ret : -EIO;
		}

		offset += ret;
		rd_iov_offset += ret;

	}

	return count;
}

/**
 * zbc_pread - Read sectors from a device
 */
ssize_t zbc_pread(struct zbc_device *dev, void *buf, size_t count,
		  uint64_t offset)
{
	const struct iovec iov = { buf, count };

	return zbc_do_preadv(dev, &iov, 1, offset);
}

/**
 * zbc_preadv - Vector read sectors from a device
 */
ssize_t zbc_preadv(struct zbc_device *dev,
		   const struct iovec *iov, int iovcnt, uint64_t offset)
{
	if (!iov || iovcnt <= 0)
		return -EINVAL;

	return zbc_do_preadv(dev, iov, iovcnt, offset);
}

/**
 * zbc_do_pwritev - Execute a vector write
 */
static ssize_t zbc_do_pwritev(struct zbc_device *dev,
			      const struct iovec *iov, int iovcnt,
			      uint64_t offset)
{
	size_t max_count = dev->zbd_info.zbd_max_rw_sectors;
	size_t count = zbc_iov_count(iov, iovcnt);
	struct iovec wr_iov[iovcnt];
	size_t wr_iov_count = 0, wr_iov_offset = 0;
	int wr_iovcnt;
	ssize_t ret;

	if (count << 9 > SSIZE_MAX)
		return -EINVAL;

	if (zbc_test_mode(dev)) {
		if (!count) {
			zbc_error("%s: zero-length write at sector %llu\n",
				  dev->zbd_filename,
				  (unsigned long long) offset);
			return -EINVAL;
		}
	} else {
		if (!zbc_dev_sect_paligned(dev, count) ||
		    !zbc_dev_sect_paligned(dev, offset)) {
			zbc_error("%s: Unaligned write %zu sectors at "
				  "sector %llu\n",
				  dev->zbd_filename,
				  count, (unsigned long long) offset);
			return -EIO;
		}

		if ((offset + count) > dev->zbd_info.zbd_sectors)
			count = dev->zbd_info.zbd_sectors - offset;
		if (!count || offset >= dev->zbd_info.zbd_sectors)
			return 0;
	}

	zbc_debug("%s: Write %zu sectors at sector %llu, %d vectors\n",
		  dev->zbd_filename,
		  count, (unsigned long long) offset, iovcnt);

	if (zbc_test_mode(dev) && count == 0) {
		zbc_iov_convert(wr_iov, iov, iovcnt, 0, &count, max_count);
		ret = (dev->zbd_drv->zbd_pwritev)(dev, wr_iov, iovcnt, offset);
		if (ret < 0) {
			zbc_error("%s: Write of zero sectors at sector %llu "
				  "failed %ld (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long) offset,
				  -ret, strerror(-ret));
		}
		return ret;
	}

	while (wr_iov_offset < count) {

		wr_iov_count = count - wr_iov_offset;
		wr_iovcnt = zbc_iov_convert(wr_iov, iov, iovcnt,
					    wr_iov_offset, &wr_iov_count,
					    max_count);

		ret = (dev->zbd_drv->zbd_pwritev)(dev, wr_iov, wr_iovcnt,
						  offset);
		if (ret <= 0) {
			zbc_error("%s: Write %zu sectors at sector %llu failed "
				  "%zd (%s)\n",
				  dev->zbd_filename,
				  wr_iov_count, (unsigned long long) offset,
				  -ret, strerror(-ret));
			return ret ? ret : -EIO;
		}

		offset += ret;
		wr_iov_offset += ret;

	}

	return count;
}

/**
 * zbc_pwrite - Write sectors form a device
 */
ssize_t zbc_pwrite(struct zbc_device *dev, const void *buf, size_t count,
		   uint64_t offset)
{
	const struct iovec iov = { (void *)buf, count };

	return zbc_do_pwritev(dev, &iov, 1, offset);
}

/**
 * zbc_pwritev - Vector write sectors form a device
 */
ssize_t zbc_pwritev(struct zbc_device *dev, const struct iovec *iov, int iovcnt,
		    uint64_t offset)
{
	if (!iov || iovcnt <= 0)
		return -EINVAL;

	return zbc_do_pwritev(dev, iov, iovcnt, offset);
}

/**
 * zbc_map_iov - Map a buffer to an IOV
 */
int zbc_map_iov(const void *buf, size_t sectors,
		struct iovec *iov, int iovcnt, size_t iovlen)
{
	size_t len;
	int i = 0;

	if (!buf || !sectors || !iov || iovcnt <= 0 ||
	    sectors > iovcnt * iovlen)
		return -EINVAL;

	while (sectors) {

		if (sectors > iovlen)
			len = iovlen;
		else
			len = sectors;

		iov[i].iov_base = (void*)buf;
		iov[i].iov_len = len;

		buf += len << 9;
		sectors -= len;
		i++;
	}

	return i;
}

/**
 * zbc_flush - flush a device write cache
 */
int zbc_flush(struct zbc_device *dev)
{
	return (dev->zbd_drv->zbd_flush)(dev);
}

/**
 * zbc_get_zbd_stats - Receive Zoned Block Device statistics
 */
int zbc_get_zbd_stats(struct zbc_device *dev,
		      struct zbc_zoned_blk_dev_stats *stats)
{
	if (!dev->zbd_drv->zbd_get_stats)
		return -ENXIO;

	return (dev->zbd_drv->zbd_get_stats)(dev, stats);
}

