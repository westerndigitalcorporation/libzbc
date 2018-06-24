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
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Christoph Hellwig (hch@infradead.org)
 */

#include "zbc.h"

#include <string.h>

/*
 * Log level.
 */
int zbc_log_level = ZBC_LOG_WARNING;

/*
 * Backend drivers.
 */
static struct zbc_drv *zbc_drv[] = {
	&zbc_block_drv,
	&zbc_scsi_drv,
	&zbc_ata_drv,
	&zbc_fake_drv,
	NULL
};

/**
 * Sense key strings.
 */
static struct zbc_sg_sk_s {
	enum zbc_sk	sk;
	const char	*sk_name;
} zbc_sg_sk_list[] = {
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

/***** Definition of public functions *****/

/**
 * zbc_set_log_level - Set the library log level
 */
void
zbc_set_log_level(char const *log_level)
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
	case ZBC_DT_BLOCK:
		return "Zoned block device";
	case ZBC_DT_SCSI:
		return "SCSI ZBC device";
	case ZBC_DT_ATA:
		return "ATA ZAC device";
	case ZBC_DT_FAKE:
		return "Emulated zoned block device";
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

	memcpy(err, &dev->zbd_errno, size);
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

/**
 * zbc_device_is_zoned - Test if a physical device is zoned.
 */
int zbc_device_is_zoned(const char *filename,
			bool fake,
			struct zbc_device_info *info)
{
	struct zbc_device *dev = NULL;
	int ret = -ENODEV, i;

	/* Test all backends until one accepts the drive. */
	for (i = 0; zbc_drv[i]; i++) {
		ret = zbc_drv[i]->zbd_open(filename, O_RDONLY, &dev);
		if (ret == 0) {
			/* This backend accepted the device */
			dev->zbd_drv = zbc_drv[i];
			break;
		}
		if (ret != -ENXIO)
			return ret;
	}

	if (dev && dev->zbd_drv) {
		if (dev->zbd_drv == &zbc_fake_drv && !fake) {
			ret = 0;
		} else {
			ret = 1;
			if (info)
				memcpy(info, &dev->zbd_info,
				       sizeof(struct zbc_device_info));
		}
		dev->zbd_drv->zbd_close(dev);
	} else {
		if ((ret != -EPERM) && (ret != -EACCES))
			ret = 0;
	}

	return ret;
}

/**
 * zbc_open - open a ZBC device
 */
int zbc_open(const char *filename, int flags, struct zbc_device **pdev)
{
	struct zbc_device *dev = NULL;
	unsigned int allowed_drv;
	int ret, i;

	allowed_drv = flags & ZBC_O_DRV_MASK;
	if (!allowed_drv)
		allowed_drv = ZBC_O_DRV_MASK;
#ifndef HAVE_LINUX_BLKZONED_H
	allowed_drv &= ~ZBC_O_DRV_BLOCK;
#endif

	/* Test all backends until one accepts the drive */
	for (i = 0; zbc_drv[i] != NULL; i++) {

		if (!(zbc_drv[i]->flag & allowed_drv))
			continue;

		ret = zbc_drv[i]->zbd_open(filename, flags, &dev);
		switch (ret) {
		case 0:
			/* This backend accepted the drive */
			dev->zbd_drv = zbc_drv[i];
			*pdev = dev;
			return 0;
		case -ENXIO:
			continue;
		default:
			return ret;
		}

	}

	return -ENODEV;
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

	if (info->zbd_model != ZBC_DM_STANDARD)
		fprintf(out, "    Zone Domains command set is %ssupported\n",
			(info->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) ? "" : "NOT ");
	if (info->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) {
		fprintf(out, "    Unrestricted read control is %ssupported\n",
			(info->zbd_flags & ZBC_URSWRZ_SET_SUPPORT) ? "" : "NOT ");
		if (info->zbd_flags & ZBC_MAXACT_SET_SUPPORT) {
			fprintf(out,
				"    Setting maximum number of zones "
				"to activate is supported\n");
		}
		if (info->zbd_max_conversion != 0)
			fprintf(out, "    Maximum number of zones to activate: %u\n",
				info->zbd_max_conversion);
		else
			fprintf(out, "    Maximum number of zones"
				     " to activate is unlimited\n");
		fprintf(out, "    REPORT REALMS command is %ssupported\n",
			(info->zbd_flags & ZBC_REPORT_REALMS_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    ZONE QUERY command is %ssupported\n",
			(info->zbd_flags & ZBC_ZONE_QUERY_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    ZA (FSNOZ) control is %ssupported\n",
			(info->zbd_flags & ZBC_ZA_CONTROL_SUPPORT) ? "" : "NOT ");
		fprintf(out, "    Supported zone types: %s%s%s%s\n",
			(info->zbd_flags & ZBC_CONV_ZONE_SUPPORT) ? "Conv " : "",
			(info->zbd_flags & ZBC_SEQ_REQ_ZONE_SUPPORT) ? "SWR " : "",
			(info->zbd_flags & ZBC_SEQ_PREF_ZONE_SUPPORT) ? "SWP " : "",
			(info->zbd_flags & ZBC_SOBR_ZONE_SUPPORT) ? "SOBR " : "");
	}

	fprintf(out, "    MUTATE command set is %ssupported\n",
		(info->zbd_flags & ZBC_MUTATE_SUPPORT) ? "" : "NOT ");

	fflush(out);
}

/**
 * zbc_report_zones - Get zone information
 */
int zbc_report_zones(struct zbc_device *dev, uint64_t sector,
		     enum zbc_reporting_options ro,
		     struct zbc_zone *zones, unsigned int *nr_zones)
{
	unsigned int n, nz = 0;
	uint64_t last_sector;
	int ret;

	if (!zones) {
		/* Get the number of zones */
		*nr_zones = 0;
		return (dev->zbd_drv->zbd_report_zones)(dev, sector,
							zbc_ro_mask(ro),
							NULL, nr_zones);
	}

	/* Get zones information */
	while (nz < *nr_zones) {

		n = *nr_zones - nz;
		ret = (dev->zbd_drv->zbd_report_zones)(dev, sector,
					zbc_ro_mask(ro) | ZBC_RO_PARTIAL,
					&zones[nz], &n);
		if (ret != 0) {
			zbc_error("%s: Get zones from sector %llu failed %d (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long) sector,
				  ret, strerror(-ret));
			return ret;
		}

		if (n == 0)
			break;

		nz += n;
		last_sector = zones[nz - 1].zbz_start +
			zones[nz - 1].zbz_length;

		if (last_sector >= dev->zbd_info.zbd_sectors)
			break;

		sector = last_sector;

	}

	*nr_zones = nz;

	return 0;
}

/**
 * zbc_list_zones - Get zone information
 */
int zbc_list_zones(struct zbc_device *dev, uint64_t sector,
		   enum zbc_reporting_options ro,
		   struct zbc_zone **pzones, unsigned int *pnr_zones)
{
	struct zbc_zone *zones = NULL;
	unsigned int nr_zones;
	int ret;

	/* Get total number of zones */
	ret = zbc_report_nr_zones(dev, sector, zbc_ro_mask(ro), &nr_zones);
	if (ret < 0)
		return ret;

	zbc_debug("%s: %d zones\n",
		  dev->zbd_filename,
		  nr_zones);

	/* Allocate zone array */
	zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));
	if (!zones)
		return -ENOMEM;

	/* Get zones information */
	ret = zbc_report_zones(dev, sector, zbc_ro_mask(ro), zones, &nr_zones);
	if (ret != 0) {
		zbc_error("%s: zbc_report_zones failed %d\n",
			  dev->zbd_filename, ret);
		free(zones);
		return ret;
	}

	*pzones = zones;
	*pnr_zones = nr_zones;

	return 0;
}

/**
 * zbc_zone_operation - Execute an operation on a zone
 */
int zbc_zone_operation(struct zbc_device *dev, uint64_t sector,
		       enum zbc_zone_op op, unsigned int flags)
{
	if (!zbc_test_mode(dev) &&
	    (!(flags & ZBC_OP_ALL_ZONES)) &&
	    !zbc_dev_sect_laligned(dev, sector))
		return -EINVAL;

	/* Execute the operation */
	return (dev->zbd_drv->zbd_zone_op)(dev, sector, op, flags);
}

/**
 * zbc_report_domains - Get zone domain information
 */
int zbc_report_domains(struct zbc_device *dev, struct zbc_zone_domain *domains,
		       unsigned int nr_domains)
{
	int ret;

	if (!zbc_dev_is_zone_dom(dev)) {
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

	ret = (dev->zbd_drv->zbd_report_domains)(dev, domains, nr_domains);
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
int zbc_list_domains(struct zbc_device *dev, struct zbc_zone_domain **pdomains,
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

	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
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
	ret = zbc_report_domains(dev, domains, ZBC_EST_ALLOC_DOMAINS);
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
		ret = zbc_report_domains(dev, domains, nr_domains);
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

/*
 * If REPORT REALMS is not supported by the device,
 * try to emulate it with REPORT ZONES and ZONE QUERY.
 */
static int zbc_emulate_report_realms(struct zbc_device *dev,
				     struct zbc_zone_realm *realms,
				     unsigned int *nr_realms)
{
	struct zbc_device_info *di = &dev->zbd_info;
	struct zbc_zone_realm *r;
	struct zbc_zone *z, *zones = NULL;
	struct zbc_conv_rec *conv_recs = NULL, *cr;
	uint64_t lba, cmr_start = 0LL, smr_start = 0LL;
	unsigned int nr_zones, nr_conv_recs = 0, zone_type, dnr, space_zones;
	unsigned int cmr_len = 0, smr_len = 0, cmr_type = 0, smr_type = 0;
	unsigned int cmr_cond = 0, smr_cond = 0;
	int i, ret = 1, seam_zone;
	bool have_cmr, have_smr;

	ret = zbc_report_nr_zones(dev, 0LL, ZBC_RO_ALL, &nr_zones);
	if (ret != 0)
		goto out;

	/* Allocate zone array */
	zones = (struct zbc_zone *)calloc(nr_zones, sizeof(struct zbc_zone));
	if (!zones) {
		ret = -ENOMEM;
		goto out;
	}

	/* Get zone information */
	ret = zbc_report_zones(dev, 0LL, ZBC_RO_ALL, zones, &nr_zones);
	if (ret != 0)
		goto out;

	/* Try to find the first SMR zone */
	z = zones;
	seam_zone = -1;
	for (i = 0; i < (int)nr_zones; i++, z++) {
		if (zbc_zone_sequential(z)) {
			seam_zone = i;
			break;
		}
	}
	if (seam_zone < 0) {
		zbc_error("%s: No seam found\n", dev->zbd_filename);
		ret = -EINVAL;
		goto out;
	}

	/* Try CMR zone space first for the query */
	if (di->zbd_flags & ZBC_CONV_ZONE_SUPPORT) {
		zone_type = ZBC_ZT_CONVENTIONAL;
	} else if (di->zbd_flags & ZBC_SOBR_ZONE_SUPPORT) {
		zone_type = ZBC_ZT_SEQ_OR_BEF_REQ;
	} else {
		zbc_error("%s: No CMR zone supported\n", dev->zbd_filename);
		ret = -ENOTSUP;
		goto out;
	}
	lba = zbc_zone_start(z);
	space_zones = nr_zones - seam_zone;

	/* Get the number of conversion records */
	ret = zbc_get_nr_cvt_records(dev, true, false, false, lba,
				     space_zones, zone_type);
	if (ret < 0) {
		/* OK, try in once again with SMR zone space */
		ret = 0;
		if (di->zbd_flags & ZBC_SEQ_REQ_ZONE_SUPPORT) {
			zone_type = ZBC_ZT_SEQUENTIAL_REQ;
		} else if (di->zbd_flags & ZBC_SEQ_PREF_ZONE_SUPPORT) {
			zone_type = ZBC_ZT_SEQUENTIAL_PREF;
		} else {
			zbc_error("%s: No SMR zone supported\n",
				  dev->zbd_filename);
			ret = -ENOTSUP;
			goto out;
		}
		lba = 0LL;
		space_zones = seam_zone;

		ret = zbc_get_nr_cvt_records(dev, true, false, false, lba,
					     space_zones, zone_type);
		if (ret < 0)
			goto out;
	}

	nr_conv_recs = (uint32_t)ret;

	/* Allocate conversion record array */
	conv_recs = (struct zbc_conv_rec *)calloc(nr_conv_recs,
						  sizeof(struct zbc_conv_rec));
	if (!conv_recs) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * Get the list of conversion records.
	 * We will report pairs of them as realms.
	 */
	ret = zbc_zone_query(dev, true, true, false, lba, space_zones,
			     zone_type, conv_recs, &nr_conv_recs);
	if (ret != 0)
		goto out;

	/* Fill zone realm information */
	dnr = 0;
	r = realms;
	have_cmr = have_smr = false;
	for (i = 0; i < (int)nr_conv_recs; i++) {
		cr = &conv_recs[i];
		if (zbc_conv_rec_nonseq(cr)) {
			if (have_cmr) {
				zbc_error("%s: Unsupported CMR CR sequence\n",
					  dev->zbd_filename);
				ret = -ENOTSUP;
				goto out;
			}
			cmr_start = cr->zbe_start_zone;
			cmr_len = cr->zbe_nr_zones;
			cmr_type = cr->zbe_type;
			cmr_cond = cr->zbe_condition;
			have_cmr = true;
		}
		if (zbc_conv_rec_seq(cr)) {
			if (have_smr) {
				zbc_error("%s: Unsupported SMR CR sequence\n",
					  dev->zbd_filename);
				ret = -ENOTSUP;
				goto out;
			}
			smr_start = cr->zbe_start_zone;
			smr_len = cr->zbe_nr_zones;
			smr_type = cr->zbe_type;
			smr_cond = cr->zbe_condition;
			have_smr = true;
		}
		if (have_cmr && have_smr) {
			if (r) {
				r->zbr_number = dnr;
				if (cmr_cond == ZBC_ZC_INACTIVE &&
				    smr_cond == ZBC_ZC_INACTIVE) {
					zbc_error("%s: Can't determine realm type\n",
						  dev->zbd_filename);
					ret = -EINVAL;
					goto out;
				}
/* FIXME new realm structure */
#if 0
				r->zbr_type = (cmr_cond == ZBC_ZC_INACTIVE) ? smr_type : cmr_type;
				r->zbr_convertible = 0;
				if (cmr_len)
					r->zbr_convertible |= ZBC_CVT_TO_CONV;
				if (smr_len)
					r->zbr_convertible |= ZBC_CVT_TO_SEQ;

				r->zbr_conv_start = zbc_dev_lba2sect(dev, cmr_start);
				r->zbr_conv_length = cmr_len;
				r->zbr_seq_start = zbc_dev_lba2sect(dev, smr_start);
				r->zbr_seq_length = smr_len;
#endif
				r++;
			}
			have_cmr = have_smr = false;
			dnr++;
		}
		if (r && dnr >= *nr_realms)
			break;
	}
	*nr_realms = dnr;

out:
	if (zones)
		free(zones);
	if (conv_recs)
		free(conv_recs);

	return ret;
}

/**
 * zbc_report_realms - Get zone realm information
 */
int zbc_report_realms(struct zbc_device *dev,
		      struct zbc_zone_realm *realms, unsigned int *nr_realms)
{
	struct zbc_device_info *di = &dev->zbd_info;
	struct zbc_zone_realm *r;
	struct zbc_zone *zones = NULL;
	uint64_t lba;
	unsigned int i, j, len;
	int ret;

	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	/* If realm array is not provided, just get the number of realms */
	if (!realms)
		*nr_realms = 0;

	/* Get zone realm information */
	if (di->zbd_flags & ZBC_REPORT_REALMS_SUPPORT)
		ret = (dev->zbd_drv->zbd_report_realms)(dev, realms, nr_realms);
	else if (di->zbd_flags & ZBC_ZONE_QUERY_SUPPORT)
		ret = zbc_emulate_report_realms(dev, realms, nr_realms);
	else
		ret = -ENOTSUP;
	if (ret != 0) {
		zbc_error("%s: REPORT REALMS failed %d (%s)\n",
			  dev->zbd_filename,
			  ret, strerror(-ret));
		return ret;
	}
	if (!realms)
		goto out;

	/* Allocate zone array */
	zones = (struct zbc_zone *)calloc(1, sizeof(struct zbc_zone));
	if (!zones) {
		fprintf(stderr, "No memory\n");
		return -ENOMEM;
	}

	/*
	 * Get information about the first zone of every realm
	 * and calculate the size of the realm in zones.
	 */
	for (i = 0; i < *nr_realms; i++) {
		r = &realms[i];
		for (j = 0; j < zbc_zone_realm_nr_domains(r); j++) {
			if (!zbc_realm_actv_as_dom_id(r, j))
				continue;
			lba = zbc_realm_start_lba(r, j);
			len = 1;
			ret = zbc_report_zones(dev, lba, 0, zones, &len);
			if (ret != 0) {
				fprintf(stderr, "zbc_report_zones failed %d\n", ret);
				goto out;
			}
			if (!len || zones->zbz_start != lba || zones->zbz_length == 0) {
				fprintf(stderr,
					"malformed zone response, start=%lu, len=%lu\n",
					zones->zbz_start, zones->zbz_length);
				goto out;
			}

			len = zbc_realm_block_length(r, j) / zones->zbz_length;
			r->zbr_ri[j].zbi_length = len;
		}
	}

out:
	if (zones)
		free(zones);

	return 0;
}

/**
 * zbc_list_zone_realms - List zone realm information
 */
int zbc_list_zone_realms(struct zbc_device *dev,
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

	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
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
	ret = zbc_report_realms(dev, realms, &nr_realms);
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
 * zbc_zone_activate - Activate all zones in a specified range to a new type
 */
int zbc_zone_activate(struct zbc_device *dev, bool zsrc,
		      bool all, bool use_32_byte_cdb,
		      uint64_t lba, unsigned int nr_zones,
		      unsigned int domain_id, struct zbc_conv_rec *conv_recs,
		      unsigned int *nr_conv_recs)
{
	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}
	if (!dev->zbd_drv->zbd_zone_query_cvt) {
		/* FIXME need to implement! */
		zbc_warning("%s: Zone activate/query is not implemented\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	/* Execute the operation */
	return (dev->zbd_drv->zbd_zone_query_cvt)(dev, zsrc,
						  all, use_32_byte_cdb,
						  false, lba, nr_zones,
						  domain_id, conv_recs,
						  nr_conv_recs);
}

/**
 * zbc_zone_query - Receive information about activating all zones in a
 *                  specific range to a new domain without actually performing
 *                  the activation process
 */
int zbc_zone_query(struct zbc_device *dev, bool zsrc,
		   bool all, bool use_32_byte_cdb,
		   uint64_t lba, unsigned int nr_zones, unsigned int domain_id,
		   struct zbc_conv_rec *conv_recs, unsigned int *nr_conv_recs)
{
	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}
	if (!dev->zbd_drv->zbd_zone_query_cvt) {
		/* FIXME need to implement! */
		zbc_warning("%s: Zone activation/query is not implemented\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	/* Execute the operation */
	return (dev->zbd_drv->zbd_zone_query_cvt)(dev, zsrc,
						  all, use_32_byte_cdb,
						  true, lba, nr_zones,
						  domain_id, conv_recs,
						  nr_conv_recs);
}

/**
 * zbc_get_nr_cvt_records - Get the expected number of conversion records
 * 			    for a ZONE ACTIVATE or ZONE QUERY operation
 */
int zbc_get_nr_cvt_records(struct zbc_device *dev, bool zsrc, bool all,
			   bool use_32_byte_cdb, uint64_t lba,
			   unsigned int nr_zones, unsigned int domain_id)
{
	uint32_t nr_conv_recs = 0;
	int ret;

	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}
	if (!dev->zbd_drv->zbd_zone_query_cvt) {
		/* FIXME need to implement! */
		zbc_warning("%s: Zone activation/query is not implemented\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	ret = (dev->zbd_drv->zbd_zone_query_cvt)(dev, zsrc,
						 all, use_32_byte_cdb,
						 true, lba, nr_zones,
						 domain_id, NULL,
						 &nr_conv_recs);
	return ret ? ret : (int)nr_conv_recs;
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
int zbc_zone_query_list(struct zbc_device *dev, bool zsrc, bool all, bool use_32_byte_cdb,
			uint64_t lba, unsigned int nr_zones,
			unsigned int domain_id,
			struct zbc_conv_rec **pconv_recs,
			unsigned int *pnr_conv_recs)
{
	struct zbc_conv_rec *conv_recs = NULL;
	uint32_t nr_conv_recs;
	int ret;

	ret = zbc_get_nr_cvt_records(dev, zsrc, all, use_32_byte_cdb,
				     lba, nr_zones, domain_id);
	if (ret < 0)
		return ret;
	nr_conv_recs = (uint32_t)ret;

	/* Allocate conversion record array */
	conv_recs = (struct zbc_conv_rec *)
		    calloc(nr_conv_recs, sizeof(struct zbc_conv_rec));
	if (!conv_recs)
		return -ENOMEM;

	/* Now get the entire list */
	ret = (dev->zbd_drv->zbd_zone_query_cvt)(dev, zsrc,
						 all, use_32_byte_cdb,
						 true, lba, nr_zones,
						 domain_id, conv_recs,
						 &nr_conv_recs);
	*pconv_recs = conv_recs;
	*pnr_conv_recs = nr_conv_recs;

	return ret;
}

/**
 * zbc_zone_activation_ctl - Get or set device DH-SMR configuration parameters
 */
int zbc_zone_activation_ctl(struct zbc_device *dev,
			    struct zbc_zp_dev_control *ctl, bool set)
{
	if (!zbc_dev_is_zone_dom(dev)) {
		zbc_error("%s: Not a Zone Domains device\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	if (!dev->zbd_drv->zbd_dev_control) {
		/* FIXME need to implement! */
		zbc_warning("%s: DH-SMR dev_ctl not implemented by driver\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	return (dev->zbd_drv->zbd_dev_control)(dev, ctl, set);
}

/**
 * zbc_pread - Read sectors from a device
 */
ssize_t zbc_pread(struct zbc_device *dev, void *buf,
		  size_t count, uint64_t offset)
{
	size_t max_count = dev->zbd_info.zbd_max_rw_sectors;
	size_t sz, rd_count = 0;
	ssize_t ret;

	if (!zbc_test_mode(dev)) {
		if (!zbc_dev_sect_laligned(dev, count) ||
		    !zbc_dev_sect_laligned(dev, offset)) {
			zbc_error("%s: Unaligned read %zu sectors at sector %llu\n",
				  dev->zbd_filename,
				  count, (unsigned long long)offset);
			return -EINVAL;
		}

		if ((offset + count) > dev->zbd_info.zbd_sectors)
			count = dev->zbd_info.zbd_sectors - offset;
		if (!count || offset >= dev->zbd_info.zbd_sectors)
			return 0;
	}

	zbc_debug("%s: Read %zu sectors at sector %llu\n",
		  dev->zbd_filename,
		  count, (unsigned long long) offset);

	if (zbc_test_mode(dev) && count == 0) {
		ret = (dev->zbd_drv->zbd_pread)(dev, buf, count, offset);
		if (ret < 0) {
			zbc_error("%s: Read of zero sectors at sector %llu failed %ld (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long) offset,
				  -ret, strerror(-ret));
		}
		return ret;
	}

	while (count) {

		if (count > max_count)
			sz = max_count;
		else
			sz = count;

		ret = (dev->zbd_drv->zbd_pread)(dev, buf, sz, offset);
		if (ret <= 0) {
			zbc_error("%s: Read %zu sectors at sector %llu failed %zd (%s)\n",
				  dev->zbd_filename,
				  sz, (unsigned long long) offset,
				  -ret, strerror(-ret));
			return ret ? ret : -EIO;
		}

		buf += ret << 9;
		offset += ret;
		count -= ret;
		rd_count += ret;

	}

	return rd_count;
}

/**
 * zbc_pwrite - Write sectors to a device
 */
ssize_t zbc_pwrite(struct zbc_device *dev, const void *buf,
		   size_t count, uint64_t offset)
{
	size_t max_count = dev->zbd_info.zbd_max_rw_sectors;
	size_t sz, wr_count = 0;
	ssize_t ret;

	if (!zbc_test_mode(dev)) {
		if (!zbc_dev_sect_paligned(dev, count) ||
		    !zbc_dev_sect_paligned(dev, offset)) {
			zbc_error("%s: Unaligned write %zu sectors at sector %llu\n",
				  dev->zbd_filename,
				  count, (unsigned long long) offset);
			return -EINVAL;
		}

		if ((offset + count) > dev->zbd_info.zbd_sectors)
			count = dev->zbd_info.zbd_sectors - offset;
		if (!count || offset >= dev->zbd_info.zbd_sectors)
			return 0;
	}

	zbc_debug("%s: Write %zu sectors at sector %llu\n",
		  dev->zbd_filename,
		  count, (unsigned long long) offset);

	if (zbc_test_mode(dev) && count == 0) {
		ret = (dev->zbd_drv->zbd_pwrite)(dev, buf, count, offset);
		if (ret < 0) {
			zbc_error("%s: Write of zero sectors at sector %llu failed %ld (%s)\n",
				  dev->zbd_filename,
				  (unsigned long long) offset,
				  -ret, strerror(-ret));
		}
		return ret;
	}

	while (count) {

		if (count > max_count)
			sz = max_count;
		else
			sz = count;

		ret = (dev->zbd_drv->zbd_pwrite)(dev, buf, sz, offset);
		if (ret <= 0) {
			zbc_error("%s: Write %zu sectors at sector %llu failed %zd (%s)\n",
				  dev->zbd_filename,
				  sz, (unsigned long long) offset,
				  -ret, strerror(-ret));
			return ret ? ret : -EIO;
		}

		buf += ret << 9;
		offset += ret;
		count -= ret;
		wr_count += ret;

	}

	return wr_count;
}

/**
 * zbc_report_mutations - Get all the mutation types and options
 * 			  supported by the device.
 */
int zbc_report_mutations(struct zbc_device *dev,
			struct zbc_supported_mutation *sm,
			unsigned int *nr_sm_recs)
{
	int ret;

	if (!zbc_supp_mutate(dev)) {
		zbc_error("%s: Device doesn't support MUTATE\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	if (!dev->zbd_drv->zbd_report_mutations) {
		/* FIXME need to implement! */
		zbc_warning("%s: REPORT MUTATIONS is not implemented in driver\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	if (!sm)
		/* Just get the number of supported options */
		return (dev->zbd_drv->zbd_report_mutations)(dev, NULL, nr_sm_recs);

	/* Get the mutation types/options */
	ret = (dev->zbd_drv->zbd_report_mutations)(dev, sm, nr_sm_recs);
	if (ret != 0) {
		zbc_error("%s: REPORT MUTATIONS failed %d (%s)\n",
			  dev->zbd_filename,
			  ret, strerror(-ret));
		return ret;
	}

	return 0;
}

int zbc_mutate(struct zbc_device *dev, enum zbc_mutation_target mt,
	       union zbc_mutation_opt opt)
{
	if (!zbc_supp_mutate(dev)) {
		zbc_error("%s: Device doesn't support MUTATE\n",
			  dev->zbd_filename);
		return -ENOTSUP;
	}

	if (!dev->zbd_drv->zbd_mutate) {
		/* FIXME need to implement! */
		zbc_warning("%s: MUTATE is not implemented in driver\n",
			    dev->zbd_filename);
		return -ENOTSUP;
	}

	return (dev->zbd_drv->zbd_mutate)(dev, mt, opt);
}

/**
 * zbc_flush - flush a device write cache
 */
int zbc_flush(struct zbc_device *dev)
{
	return (dev->zbd_drv->zbd_flush)(dev);
}

/**
 * zbc_set_zones - Configure zones of an emulated device
 */
int zbc_set_zones(struct zbc_device *dev,
		  uint64_t conv_sz, uint64_t zone_sz)
{

	/* Do this only if supported */
	if (!dev->zbd_drv->zbd_set_zones)
		return -ENXIO;

	if (!zbc_dev_sect_paligned(dev, conv_sz) ||
	    !zbc_dev_sect_paligned(dev, zone_sz))
		return -EINVAL;

	return (dev->zbd_drv->zbd_set_zones)(dev, conv_sz, zone_sz);
}

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 */
int zbc_set_write_pointer(struct zbc_device *dev,
			  uint64_t sector, uint64_t wp_sector)
{

	/* Do this only if supported */
	if (!dev->zbd_drv->zbd_set_wp)
		return -ENXIO;

	if (!zbc_dev_sect_paligned(dev, sector) ||
	    !zbc_dev_sect_paligned(dev, wp_sector))
		return -EINVAL;

	return (dev->zbd_drv->zbd_set_wp)(dev, sector, wp_sector);
}

