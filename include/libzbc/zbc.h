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
 *          Christophe Louargant (christophe.louargant@wdc.com)
 */

#ifndef _LIBZBC_H_
#define _LIBZBC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>

/**
 * @mainpage
 *
 * libzbc is a simple library  providing functions for manipulating disks
 * supporting the Zoned Block Command  (ZBC) and Zoned-device ATA command
 * set (ZAC)  disks.  libzbc  implementation is  compliant with  the latest
 * drafts  of the  ZBC  and  ZAC standards  defined  by INCITS  technical
 * committee T10 and T13 (respectively).
 */

/**
 * \addtogroup libzbc
 *  @{
 */

/**
 * @brief Set the library log level
 * @param[in] log_level	Library log level
 *
 * Set the library log level using the level name specified by \a log_level.
 * Log level are incremental: each level includes the levels preceding it.
 * Valid log level names are:
 * "none"    : Silent operation (no messages)
 * "warning" : Print device level standard compliance problems
 * "error"   : Print messages related to unexpected errors
 * "info"    : Print normal information messages
 * "debug"   : Verbose output describing internally executed commands
 * The default level is "warning".
 */
extern void zbc_set_log_level(char const *log_level);

/**
 * @brief Device handle (opaque data structure).
 */
struct zbc_device;

/**
 * @brief Zone type definitions
 *
 * Indicates the type of a zone.
 */
enum zbc_zone_type {

	/**
	 * Unknown zone type.
	 */
	ZBC_ZT_UNKNOWN		= 0x00,

	/**
	 * Conventional zone.
	 */
	ZBC_ZT_CONVENTIONAL	= 0x01,

	/**
	 * Sequential write required zone: a write pointer zone
	 * that must be written sequentially (host-managed drives only).
	 */
	ZBC_ZT_SEQUENTIAL_REQ	= 0x02,

	/**
	 * Sequential write preferred zone: a write pointer zone
	 * that can be written randomly (host-aware drives only).
	 */
	ZBC_ZT_SEQUENTIAL_PREF	= 0x03,

	/**
	 * Sequential or before required zone: requires additional
	 * initialization to become close to a regular conventional,
	 * but it can be activated from SMR quickly.
	 */
	ZBC_ZT_SEQ_OR_BEF_REQ	= 0x04,

	/**
	 * Gap zone. Gaps are allowed between zone domains.
	 */
	ZBC_ZT_GAP		= 0x05,
};

/**
 * @brief returns a string describing a zone type
 * @param[in] type	Zone type
 *
 * @return A string describing a zone type.
 */
extern const char *zbc_zone_type_str(enum zbc_zone_type type);

/**
 * @brief Zone condition definitions
 *
 * A zone condition is determined by the zone type and the ZBC zone state
 * machine, i.e. the operations performed on the zone.
 */
enum zbc_zone_condition {

	/**
	 * Not a write pointer zone (i.e. a conventional zone).
	 */
	ZBC_ZC_NOT_WP		= 0x00,

	/**
	 * Empty sequential zone (zone not written too since last reset).
	 */
	ZBC_ZC_EMPTY		= 0x01,

	/**
	 * Implicitly open zone (i.e. a write command was issued to
	 * the zone).
	 */
	ZBC_ZC_IMP_OPEN		= 0x02,

	/**
	 * Explicitly open zone (a write pointer zone was open using
	 * the OPEN ZONE command).
	 */
	ZBC_ZC_EXP_OPEN		= 0x03,

	/**
	 * Closed zone (a write pointer zone that was written to and closed
	 * using the CLOSE ZONE command).
	 */
	ZBC_ZC_CLOSED		= 0x04,

	/**
	 * Inactive zone: an unmapped zone of a Zone Domains device.
	 */
	ZBC_ZC_INACTIVE		= 0x05,

	/**
	 * Read-only zone: any zone that can only be read.
	 */
	ZBC_ZC_RDONLY		= 0x0d,

	/**
	 * Full zone (write pointer zones only).
	 */
	ZBC_ZC_FULL		= 0x0e,

	/**
	 * Offline zone: unusable zone.
	 */
	ZBC_ZC_OFFLINE		= 0x0f,

};

/**
 * @brief Returns a string describing a zone condition
 * @param[in] cond	Zone condition
 *
 * @return A string describing a zone condition.
 */
extern const char *zbc_zone_condition_str(enum zbc_zone_condition cond);

/**
 * @brief Zone attributes definitions
 *
 * Defines the attributes of a zone. Attributes validity depend on the
 * zone type and device model.
 */
enum zbc_zone_attributes {

	/**
	 * Reset write pointer recommended: a write pointer zone for which
	 * the device determined that a RESET WRITE POINTER command execution
	 * is recommended. The drive level condition resulting in this
	 * attribute being set depend on the drive model/vendor and not
	 * defined by the ZBC/ZAC specifications.
	 */
	ZBC_ZA_RWP_RECOMMENDED	= 0x0001,

	/**
	 * Non-Sequential Write Resources Active: indicates that a
	 * sequential write preferred zone (host-aware devices only) was
	 * written at a random LBA (not at the write pointer position).
	 * The drive may reset this attribute at any time after the random
	 * write operation completion.
	 */
	ZBC_ZA_NON_SEQ		= 0x0002,
};

/**
 * @brief Zone information data structure
 *
 * Provide all information of a zone (position and size, condition and
 * attributes). This data structure is updated using the zbc_report_zones
 * function.
 * In order to unifies handling of zone information for devices with different
 * logical block sizes, zone start, length and write pointer position are
 * reported in unit of 512B sectors, regardless of the actual drive logical
 * block size.
 */
struct zbc_zone {

	/**
	 * Zone length in number of 512B sectors.
	 */
	uint64_t		zbz_length;

	/**
	 * First sector of the zone (512B sector unit).
	 */
	uint64_t		zbz_start;

	/**
	 * Zone write pointer sector position (512B sector unit).
	 */
	uint64_t		zbz_write_pointer;

	/**
	 * Zone type (enum zbc_zone_type).
	 */
	uint8_t			zbz_type;

	/**
	 * Zone condition (enum zbc_zone_condition).
	 */
	uint8_t			zbz_condition;

	/**
	 * Zone attributes (enum zbc_zone_attributes).
	 */
	uint8_t			zbz_attributes;

	/**
	 * Padding to 32 bytes.
	 */
	uint8_t			__pad[5];

};

/** @brief Get a zone type */
#define zbc_zone_type(z)	((int)(z)->zbz_type)

/** @brief Test if a zone type is conventional */
#define zbc_zone_conventional(z) ((z)->zbz_type == ZBC_ZT_CONVENTIONAL)

/** @brief Test if a zone type is sequential write required */
#define zbc_zone_sequential_req(z) ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_REQ)

/** @brief Test if a zone type is sequential write preferred */
#define zbc_zone_sequential_pref(z) ((z)->zbz_type == ZBC_ZT_SEQUENTIAL_PREF)

/** @brief Test if a zone type is sequential or before required (SOBR) */
#define zbc_zone_sobr(z) ((z)->zbz_type == ZBC_ZT_SEQ_OR_BEF_REQ)

/** @brief Test if a zone type is sequential write required or preferred */
#define zbc_zone_sequential(z)	(zbc_zone_sequential_req(z) || \
				 zbc_zone_sequential_pref(z))

/** @brief Test if a zone type is gap */
#define zbc_zone_gap(z)		((z)->zbz_type == ZBC_ZT_GAP)

/** @brief Get a zone condition */
#define zbc_zone_condition(z)	((int)(z)->zbz_condition)

/** @brief Test if a zone condition is "not write pointer zone" */
#define zbc_zone_not_wp(z)	((z)->zbz_condition == ZBC_ZC_NOT_WP)

/** @brief Test if a zone condition is empty */
#define zbc_zone_empty(z)	((z)->zbz_condition == ZBC_ZC_EMPTY)

/** @brief Test if a zone condition is implicit open */
#define zbc_zone_imp_open(z)	((z)->zbz_condition == ZBC_ZC_IMP_OPEN)

/** @brief Test if a zone condition is explicit open */
#define zbc_zone_exp_open(z)	((z)->zbz_condition == ZBC_ZC_EXP_OPEN)

/** @brief Test if a zone condition is explicit or implicit open */
#define zbc_zone_is_open(z)	(zbc_zone_imp_open(z) || \
				 zbc_zone_exp_open(z))

/** @brief Test if a zone condition is closed */
#define zbc_zone_closed(z)	((z)->zbz_condition == ZBC_ZC_CLOSED)

/** @brief Test if a zone condition is full */
#define zbc_zone_full(z)	((z)->zbz_condition == ZBC_ZC_FULL)

/** @brief Test if a zone condition is read-only */
#define zbc_zone_rdonly(z)	((z)->zbz_condition == ZBC_ZC_RDONLY)

/** @brief Test if a zone condition is offline */
#define zbc_zone_offline(z)	((z)->zbz_condition == ZBC_ZC_OFFLINE)

/** @brief Test if a zone condition is inactive */
#define zbc_zone_inactive(z)	((z)->zbz_condition == ZBC_ZC_INACTIVE)

/** @brief Test if a zone has the reset recommended flag set */
#define zbc_zone_rwp_recommended(z) ((z)->zbz_attributes & \
				     ZBC_ZA_RWP_RECOMMENDED)

/** @brief Test if a zone has the non-sequential write resource allocated flag set */
#define zbc_zone_non_seq(z)	((z)->zbz_attributes & ZBC_ZA_NON_SEQ)

/** @brief Get a zone start 512B sector */
#define zbc_zone_start(z)	((unsigned long long)(z)->zbz_start)

/** @brief Get a zone number of 512B sectors */
#define zbc_zone_length(z)	((unsigned long long)(z)->zbz_length)

/** @brief Get a zone write pointer 512B sector position */
#define zbc_zone_wp(z)		((unsigned long long)(z)->zbz_write_pointer)

/**
 * @brief Zone domain flags
 */
enum zbc_zone_domain_flags {
	ZBC_ZDF_SHIFTING_BOUNDARIES	= 1 << 0,
	ZBC_ZDF_VALID_ZONE_TYPE		= 1 << 1,
};

/**
 * @brief Zone domain descriptor
 *
 * Provide all information about a single zone domain supported by the
 * device. This structure is populated with the information returned to
 * the client after successful execution of REPORT ZONE DOMAINS SCSI command
 * or REPORT DOMAINS DMA ATA command.
 */
struct zbc_zone_domain {

	/**
	 * Start 512B sector of this zone domain.
	 */
	uint64_t		zbm_start_sector;

	/**
	 * End 512B sector of this zone domain.
	 */
	uint64_t		zbm_end_sector;

	/**
	 * The number of zones in this zone domain.
	 */
	uint32_t		zbm_nr_zones;

	/**
	 * Domain ID. Zone domains are numbered from 0 by
	 * the server, incrementing in ascending order by 1.
	 */
	uint8_t			zbm_id;

	/**
	 * All zones activated in the LBA range of this
	 * domain will be of this type.
	 */
	uint8_t			zbm_type;

	/**
	 * Domain flags. See enum zbc_zone_domain_flags
	 * for the flag definitions.
	 */
	uint16_t		zbm_flags;

	/**
	 * Padding to 24 bytes.
	 */
	uint8_t			__pad[4];
};

/** @brief Get zone domain ID */
static inline unsigned int zbc_zone_domain_id(struct zbc_zone_domain *d)
{
	return d->zbm_id;
}

/** @brief Get zone domain type */
static inline unsigned int zbc_zone_domain_type(struct zbc_zone_domain *d)
{
	return d->zbm_type;
}

/** @brief Get zone domain start 512B sector */
static inline uint64_t zbc_zone_domain_start_sect(struct zbc_zone_domain *d)
{
	return d->zbm_start_sector;
}

/** @brief Get zone domain start logical block */
extern uint64_t zbc_zone_domain_start_lba(struct zbc_device *dev,
					  struct zbc_zone_domain *d);

/** @brief Get zone domain end logical block */
extern uint64_t zbc_zone_domain_end_lba(struct zbc_device *dev,
					struct zbc_zone_domain *d);

/** @brief Get zone domain end 512B sector */
static inline uint64_t zbc_zone_domain_end_sect(struct zbc_zone_domain *d)
{
	return d->zbm_end_sector;
}

/** @brief Get zone domain highest 512B sector */
extern uint64_t zbc_zone_domain_high_sect(struct zbc_device *dev,
					  struct zbc_zone_domain *d);

/** @brief Get zone domain number of zones */
static inline unsigned int zbc_zone_domain_nr_zones(struct zbc_zone_domain *d)
{
	return d->zbm_nr_zones;
}

/** @brief Get zone domain size in 512B sectors */
static inline uint64_t zbc_zone_domain_sect_size(struct zbc_zone_domain *d)
{
	return zbc_zone_domain_end_sect(d) -
	       zbc_zone_domain_start_sect(d) + 1LL;
}

static inline unsigned int zbc_zone_domain_zone_size(struct zbc_zone_domain *d)
{
	return (d->zbm_end_sector + 1LL - d->zbm_start_sector) /
		d->zbm_nr_zones;
}

static inline unsigned int zbc_zone_domain_flags(struct zbc_zone_domain *d)
{
	return d->zbm_flags;
}

/**
 * Realm restriction bits. These are realm attributes that are reported
 * by the device to indicate that certain operations are not allowed for
 * zones associated with the realm. There are currently activation/deactivation
 * restrictions that can be reported as well as restrictions on write pointer
 * reset.
 */
#define ZBC_RESTRICT_ZONE_ACTIVATE	0x01 /* No activate/deactivate */
#define ZBC_RESTRICT_WP_RESET		0x02 /* No write pointer reset */

/**
 * The number of domain slots in a realm. Each slot corresponds
 * to a zone domain with a distinctive zone type.
 */
#define ZBC_NR_ZONE_TYPES	4

/**
 * @brief Zone realm item
 *
 * Provides information about a single domain in a zone realm.
 *
*/
struct zbc_realm_item {

	/**
	 * Start 512B sector for this domain.
	 */
	uint64_t		zbi_start_sector;

	/**
	 * End 512B sector for this domain.
	 */
	uint64_t		zbi_end_sector;

	/**
	 * Length in zones. Not provided by REPORT REALMS,
	 * but calculated for convenience.
	 */
	uint32_t		zbi_length;

	/**
	 * Domain ID.
	 */
	uint8_t			zbi_dom_id;

	/**
	 * The corresponding zone type. This one is provided
	 * by REPORT ZONE DOMAINS, not REPORT REALMS.
	 */
	uint8_t			zbi_type;

	/**
	 * Padding to 24 bytes.
	 */
	uint8_t			__pad[2];
};

/**
 * @brief Zone realm descriptor
 *
 * Provide all information about a single zone realm defined by the
 * device. This structure is typically populated with the information
 * returned to the client after successful execution of REPORT REALMS
 * SCSI command or REPORT REALMS DMA ATA command.
 */
struct zbc_zone_realm {

	/**
	 * Zone realm ID as returned by REPORT REALMS.
	 * The lowest is 0.
	 */
	uint16_t		zbr_number;

	/**
	 * The currently active domain ID. This is the type of all
	 * zones in the realm (enum zbc_zone_type).
	 */
	uint8_t			zbr_dom_id;

	/**
	 * Current realm zone type. This the type of all zones
	 * in the realm (enum zbc_zone_type).
	 */
	uint8_t			zbr_type;

	/**
	 * A set of flags indicating what zone types can be
	 * activated in this realm.
	 */
	uint8_t			zbr_actv_flags;

	/**
	 * The number of valid items in \a zbr_ri array below.
	 */
	uint8_t			zbr_nr_domains;

	/**
	 * Realm restrictions.
	 */
	uint8_t			zbr_restr;

	/**
	 * Padding to 8 bytes.
	 */
	uint8_t			__pad[2];

	/**
	 * Array of realm items. Depending on the number of domains,
	 * some of the entries in this array may be empty.
	 */
	struct zbc_realm_item	zbr_ri[ZBC_NR_ZONE_TYPES];
};

/** @brief Get the zone realm number */
static inline int zbc_zone_realm_number(struct zbc_zone_realm *r)
{
	return r->zbr_number;
}

/** @brief Get the zone realm domain ID */
static inline int zbc_zone_realm_domain(struct zbc_zone_realm *r)
{
	return r->zbr_dom_id;
}

/** @brief Get the zone realm type */
static inline int zbc_zone_realm_type(struct zbc_zone_realm *r)
{
	return r->zbr_type;
}

/** @brief Test if a zone realm type is CONVENTIONAL */
static inline bool zbc_zone_realm_conventional(struct zbc_zone_realm *r)
{
	return (r->zbr_type == ZBC_ZT_CONVENTIONAL);
}

/** @brief Get activation flags of a realm */
static inline uint8_t zbc_zone_realm_actv_flags(struct zbc_zone_realm *r)
{
	return r->zbr_actv_flags;
}

/** @brief Get restriction attributes of a realm */
static inline uint8_t zbc_zone_realm_restrictions(struct zbc_zone_realm *r)
{
	return r->zbr_restr;
}

/** @brief Get the number of valid domain records in a realm */
static inline
unsigned int zbc_zone_realm_nr_domains(struct zbc_zone_realm *r)
{
	return r->zbr_nr_domains;
}

/** @brief Test if a zone realm type is SEQUENTIAL OR BEFORE REQUIRED */
static inline bool zbc_zone_realm_sobr(struct zbc_zone_realm *r)
{
	return (r->zbr_type == ZBC_ZT_SEQ_OR_BEF_REQ);
}

/** @brief Test if a zone realm type is SEQUENTIAL WRITE REQUIRED */
static inline bool zbc_zone_realm_sequential(struct zbc_zone_realm *r)
{
	return (r->zbr_type == ZBC_ZT_SEQUENTIAL_REQ);
}

/** @brief Test if a zone realm type is SEQUENTIAL WRITE PREFERRED */
static inline bool zbc_zone_realm_seq_pref(struct zbc_zone_realm *r)
{
	return (r->zbr_type == ZBC_ZT_SEQUENTIAL_PREF);
}

/** @brief Get realm zone type for a particular domain */
static inline unsigned int zbc_realm_zone_type(struct zbc_zone_realm *r,
					       unsigned int dom_id)
{
	return r->zbr_ri[dom_id].zbi_type;
}

/** @brief Get the start 512B sector of a realm for a particular domain */
static inline uint64_t zbc_realm_start_sector(struct zbc_zone_realm *r,
					      unsigned int dom_id)
{
	return r->zbr_ri[dom_id].zbi_start_sector;
}

/** @brief Get realm start logical block for a particular domain */
extern uint64_t zbc_realm_start_lba(struct zbc_device *dev,
				    struct zbc_zone_realm *r,
				    unsigned int dom_id);

/** @brief Get the end 512B sector of a realm for a particular domain */
static inline uint64_t zbc_realm_end_sector(struct zbc_zone_realm *r,
					    unsigned int dom_id)
{
	return r->zbr_ri[dom_id].zbi_end_sector;
}

/** @brief Get realm end logical block for a particular domain */
extern uint64_t zbc_realm_end_lba(struct zbc_device *dev,
				  struct zbc_zone_realm *r,
				  unsigned int dom_id);

/** @brief Get realm highest 512B sector for a particular domain */
extern uint64_t zbc_realm_high_sector(struct zbc_device *dev,
				      struct zbc_zone_realm *r,
				       unsigned int dom_id);

/** @brief Get realm length in 512B sectors for a particular domain */
static inline uint64_t zbc_realm_sector_length(struct zbc_zone_realm *r,
					       unsigned int dom_id)
{
	return zbc_realm_end_sector(r, dom_id) -
	       zbc_realm_start_sector(r, dom_id) + 1LL;
}

/** @brief Get realm length in logical blocks for a particular domain */
static inline uint64_t zbc_realm_lblock_length(struct zbc_device *dev,
					       struct zbc_zone_realm *r,
					       unsigned int dom_id)
{
	return zbc_realm_end_lba(dev, r, dom_id) -
	       zbc_realm_start_lba(dev, r, dom_id) + 1LL;
}

/** @brief Get the realm length in zones for a particular domain */
static inline uint32_t zbc_realm_length(struct zbc_zone_realm *r,
					unsigned int dom_id)
{
	return r->zbr_ri[dom_id].zbi_length;
}

/** @brief Test if the zone realm can be activated/deactivated at all */
static inline bool zbc_realm_activation_allowed(struct zbc_zone_realm *r)
{
	return !(r->zbr_restr & ZBC_RESTRICT_ZONE_ACTIVATE);
}

/** @brief Test if zones of the realm can be reset */
static inline bool zbc_realm_wp_reset_allowed(struct zbc_zone_realm *r)
{
	return !(r->zbr_restr & ZBC_RESTRICT_WP_RESET);
}

static inline bool zbc_realm_actv_as_dom_id(struct zbc_zone_realm *r,
					    unsigned int dom_id)
{
	return (bool)(r->zbr_actv_flags & (1 << dom_id));
}

/** @brief Test if the zone realm can be activated as the specified zone type */
static inline bool zbc_realm_actv_as_type(struct zbc_zone_realm *r,
					  enum zbc_zone_type zt)
{
	int i;

	for (i = 0; i < r->zbr_nr_domains; i++) {
		if (zt == r->zbr_ri[i].zbi_type)
			return (bool)(r->zbr_actv_flags & (1 << i));
	}

	return false;
}

/** @brief Test if the zone realm can be activated as a conventional zone type */
static inline bool zbc_zone_realm_actv_as_conv(struct zbc_zone_realm *r)
{
	int i;

	for (i = 0; i < r->zbr_nr_domains; i++) {
		if ((r->zbr_ri[i].zbi_type == ZBC_ZT_CONVENTIONAL ||
		    r->zbr_ri[i].zbi_type == ZBC_ZT_SEQ_OR_BEF_REQ) &&
		    (r->zbr_actv_flags & (1 << i)))
			return true;
	}

	return false;
}

/** @brief Test if the zone realm can be activated as a sequential zone type */
static inline bool zbc_zone_realm_actv_as_seq(struct zbc_zone_realm *r)
{
	int i;

	for (i = 0; i < r->zbr_nr_domains; i++) {
		if ((r->zbr_ri[i].zbi_type == ZBC_ZT_SEQUENTIAL_REQ ||
		    r->zbr_ri[i].zbi_type == ZBC_ZT_SEQUENTIAL_PREF) &&
		    (r->zbr_actv_flags & (1 << i)))
			return true;
	}

	return false;
}

/** @brief Get the realm item that corresponds to the specified zone type */
static inline
struct zbc_realm_item *zbc_realm_item_by_type(struct zbc_zone_realm *r,
					      enum zbc_zone_type zt)
{
	int i;

	for (i = 0; i < r->zbr_nr_domains; i++) {
		if (zt == r->zbr_ri[i].zbi_type)
			return &r->zbr_ri[i];
	}

	return NULL;
}

/**
 * @brief Zone Activation Results record.
 *
 * A list of these descriptors is returned by ZONE ACTIVATE or ZONE QUERY
 * command to provide the caller with zone IDs and other information about
 * the activated zones.
 */
struct zbc_actv_res {

	/**
	 * @brief Starting zone ID.
	 */
	uint64_t		zbe_start_zone;

	/**
	 * @brief Number of contiguous activated zones.
	 */
	uint64_t		zbe_nr_zones;

	/**
	 * @brief Domain ID of all zones in this range.
	 */
	uint8_t			zbe_domain;

	/**
	 * @brief Zone type of all zones in this range.
	 */
	uint8_t			zbe_type;

	/**
	 * @brief Zone condition of all zones in this range.
	 */
	uint8_t			zbe_condition;

};

/** @brief Get activation results record type */
#define zbc_actv_res_type(r)		((int)(r)->zbe_type)

/** @brief Test if activation results record type is conventional */
#define zbc_actv_res_conventional(r)	((r)->zbe_type == ZBC_ZT_CONVENTIONAL)

/** @brief Test if activation results record type is sequential write required */
#define zbc_actv_res_seq_req(r)		((r)->zbe_type == ZBC_ZT_SEQUENTIAL_REQ)

/** @brief Test if activation results record type is sequential write preferred */
#define zbc_actv_res_seq_pref(r)	((r)->zbe_type == ZBC_ZT_SEQUENTIAL_PREF)

/** @brief Test if activation record type is sequential or before required (SOBR) */
#define zbc_actv_res_sobr(r)		((r)->zbe_type == ZBC_ZT_SEQ_OR_BEF_REQ)

/** @brief Test if activation results record type is conventional of SOBR */
#define zbc_actv_res_nonseq(r)		(zbc_actv_res_conventional(r) || \
					 zbc_actv_res_sobr(r))

/** @brief Test if activation record type is sequential write required or preferred */
#define zbc_actv_res_seq(r)		(zbc_actv_res_seq_req(r) || \
					 zbc_actv_res_seq_pref(r))

/**
 * @brief Zone Domains device control structure.
 *
 * The contents of this structure mirror fields in
 * ZONE DOMAINS Mode page.
 */
struct zbc_zd_dev_control {
	/**
	 * @brief Default number of zones to activate.
	 */
	uint32_t		zbt_nr_zones;

	/**
	 * @brief Maximum number of LBA realms that can be activated at once.
	 */
	uint16_t		zbt_max_activate;

	/**
	 * @brief URSWRZ setting. Zero value means off.
	 *
	 * FIXME setting URSWRZ this way is vendor-specific.
	 * A standard method should be eventually defined.
	 */
	uint8_t			zbt_urswrz;

};

/**
 * Vendor ID string maximum length.
 */
#define ZBC_DEVICE_INFO_LENGTH  32

/**
 * @brief Device type definitions
 *
 * Each type correspond to a different internal backend driver.
 */
enum zbc_dev_type {

	/**
	 * Unknown drive type.
	 */
	ZBC_DT_UNKNOWN	= 0x00,

	/**
	 * SCSI device.
	 */
	ZBC_DT_SCSI	= 0x02,

	/**
	 * ATA device.
	 */
	ZBC_DT_ATA	= 0x03,

};

/**
 * @brief Returns a device type name
 * @param[in] type	Device type
 *
 * @return A string describing the interface type of a device.
 */
extern const char *zbc_device_type_str(enum zbc_dev_type type);

/**
 * @brief Device model definitions
 *
 * Indicates the ZBC/ZAC device zone model, i.e host-aware, host-managed,
 * device-managed or standard. Note that these last two models are not
 * handled by libzbc (the device will be treated as a regular block device
 * as it should).
 *   - Host-managed: device type 14h
 *   - Host-aware: device type 0h and zoned field equal to 01b
 *   - Device-managed: device type 0h and zoned field equal to 10b
 *   - Standard: device type 0h (standard block device)
 */
enum zbc_dev_model {

	/**
	 * Unknown drive model.
	 */
	ZBC_DM_DRIVE_UNKNOWN	= 0x00,

	/**
	 * Host-aware drive model: the device type/signature is 0x00
	 * and the ZONED field of the block device characteristics VPD
	 * page B1h is 01b.
	 */
	ZBC_DM_HOST_AWARE	= 0x01,

	/**
	 * Host-managed drive model: the device type/signature is 0x14/0xabcd.
	 */
	ZBC_DM_HOST_MANAGED	= 0x02,

	/**
	 * Drive-managed drive model: the device type/signature is 0x00
	 * and the ZONED field of the block device characteristics VPD
	 * page B1h is 10b.
	 */
	ZBC_DM_DEVICE_MANAGED	= 0x03,

	/**
	 * Standard block device: the device type/signature is 0x00
	 * and the ZONED field of the block device characteristics VPD
	 * page B1h is 00b.
	 */
	ZBC_DM_STANDARD		= 0x04,

};

/**
 * @brief Returns a device zone model name
 * @param[in] model	Device model
 *
 * @return A string describing a device model.
 */
extern const char *zbc_device_model_str(enum zbc_dev_model model);

/**
 * @brief Device flags definitions.
 *
 * Defines device information flags.
 */
enum zbc_dev_flags {

	/**
	 * Indicates that a device has unrestricted read operation,
	 * i.e. that read commands spanning a zone write pointer or two
	 * consecutive zones of the same type will not result in an error.
	 */
	ZBC_UNRESTRICTED_READ = 0x00000001,

	/**
	 * Indicates that the device supports Zone Realms command set
	 * to allow zones on the device to be activated both as CMR and SMR.
	 */
	ZBC_ZONE_REALMS_SUPPORT = 0x00000002,

	/**
	 * Indicates that the device supports Zone Domains command set
	 * to allow zones on the device to be activated both as CMR and SMR.
	 */
	ZBC_ZONE_DOMAINS_SUPPORT = 0x00000004,

	/**
	 * Indicates that modification of the URSWRZ setting is supported.
	 */
	ZBC_URSWRZ_SET_SUPPORT = 0x00000008,

	/**
	 * Indicates that modification of MAXIMUM ACTIVATION is supported by device.
	 */
	ZBC_MAXACT_SET_SUPPORT = 0x00000010,

	/**
	 * Indicates that REPORT REALMS command is supported by device.
	 */
	ZBC_REPORT_REALMS_SUPPORT = 0x00000020,

	/**
	 * Indicates that setting FSNOZ value is supported by device.
	 */
	ZBC_ZA_CONTROL_SUPPORT = 0x00000080,

	/**
	 * Indicates that NOZSRC bit in ZONE ACTIVATE/QUERY
	 * is supported by device.
	 */
	ZBC_NOZSRC_SUPPORT = 0x000000100,

	/**
	 * Indicates that Conventional zone type is supported by device.
	 */
	ZBC_CONV_ZONE_SUPPORT = 0x00000200,

	/**
	 * Indicates that Sequential Write Required zone type is supported.
	 */
	ZBC_SEQ_REQ_ZONE_SUPPORT = 0x00000400,

	/**
	 * Indicates that Sequential Write Preferred zone type is supported.
	 */
	ZBC_SEQ_PREF_ZONE_SUPPORT = 0x00000800,

	/**
	 * Indicates that Sequential Or Before Required zone type is supported.
	 */
	ZBC_SOBR_ZONE_SUPPORT = 0x00001000,

	/**
	 * Indicates that Gap zone type is supported, i.e. gaps
	 * are possible between domains.
	 */
	ZBC_GAP_ZONE_SUPPORT = 0x00002000,

	/**
	 * Indicates that the Conventional domain has shifting realm boundaries.
	 */
	ZBC_CONV_REALMS_SHIFTING = 0x00004000,

	/**
	 * Indicates that the Sequential Write Required domain has shifting realm boundaries.
	 */
	ZBC_SEQ_REQ_REALMS_SHIFTING = 0x00008000,

	/**
	 * Indicates that the Sequential Write Preferred domain has shifting realm boundaries.
	 */
	ZBC_SEQ_PREF_REALMS_SHIFTING = 0x00010000,

	/**
	 * Indicates that the Sequential Or Before Required domain has shifting realm boundaries.
	 */
	ZBC_SOBR_REALMS_SHIFTING = 0x00020000,

	/**
	 * Indicates that the device supports ZAC-2 zone operation counts.
	 */
	ZBC_ZONE_OP_COUNT_SUPPORT = 0x00040000,

	/**
	 * Indicates that the device supports the standard ZAC-2 REPORT REALMS data layout.
	 */
	ZBC_STANDARD_RPT_REALMS = 0x00080000,
};

/**
 * "not reported" value for the number of zones limits in the device
 * information (zbd_opt_nr_non_seq_write_seq_pref and zbd_max_nr_open_seq_req).
 */
#define ZBC_NOT_REPORTED	((uint32_t)0xFFFFFFFF)

/**
 * "no limit" value for the number of explicitly open sequential write required
 * zones in the device information (zbd_max_nr_open_seq_req).
 */
#define ZBC_NO_LIMIT		((uint32_t)0xFFFFFFFF)

/**
 * @brief Device information data structure
 *
 * Provide information on a device open using the \a zbc_open function.
 */
struct zbc_device_info {

	/**
	 * Device type.
	 */
	enum zbc_dev_type	zbd_type;

	/**
	 * Device model.
	 */
	enum zbc_dev_model	zbd_model;

	/**
	 * Device vendor, model and firmware revision string.
	 */
	char			zbd_vendor_id[ZBC_DEVICE_INFO_LENGTH];

	/**
	 * Device flags (enum zbc_dev_flags).
	 */
	uint32_t		zbd_flags;

	/**
	 * Total number of 512B sectors of the device.
	 */
	uint64_t		zbd_sectors;

	/**
	 * Size in bytes of the device logical blocks.
	 */
	uint32_t		zbd_lblock_size;

	/**
	 * Total number of logical blocks of the device.
	 */
	uint64_t		zbd_lblocks;

	/**
	 * Size in bytes of the device physical blocks.
	 */
	uint32_t		zbd_pblock_size;

	/**
	 * Total number of physical blocks of the device.
	 */
	uint64_t		zbd_pblocks;

	/**
	 * The maximum number of 512B sectors that can be
	 * transferred with a single command to the device.
	 */
	uint64_t		zbd_max_rw_sectors;

	/**
	 * Optimal maximum number of explicitly open sequential write
	 * preferred zones (host-aware device models only). A value
	 * of "-1" means that the drive did not report any value.
	 */
	uint32_t		zbd_opt_nr_open_seq_pref;

	/**
	 * Optimal maximum number of sequential write preferred zones
	 * with the ZBC_ZA_NON_SEQ zone attribute set
	 * (host-aware device models only). A value of "-1" means that
	 * the drive did not report any value.
	 */
	uint32_t		zbd_opt_nr_non_seq_write_seq_pref;

	/**
	 * Maximum number of explicitly open sequential write required
	 * zones (host-managed device models only). A value of "-1" means
	 * that there is no restrictions on the number of open zones.
	 */
	uint32_t		zbd_max_nr_open_seq_req;

	/**
	 * Maximum allowable value for NUMBER OF ZONES value in
	 * ZONE ACTIVATE or ZONE QUERY command. Zero means no maximum.
	 */
	uint32_t		zbd_max_activation;

	/**
	 * Subsequent Number of Zones. This is the current value of
	 * NUMBER OF ZONES value set in Zone Activation control.
	 */
	uint32_t		zbd_snoz;

};

/**
 * @brief Test is this device supports Zone Domains or Zone Realms
 */
static inline bool zbc_device_is_zdr(struct zbc_device_info *info)
{
	return (info->zbd_flags & ZBC_ZONE_DOMAINS_SUPPORT) ||
	       (info->zbd_flags & ZBC_ZONE_REALMS_SUPPORT);
}

/**
 * @brief: Test if this device supports non-zero COUNT value
 * in zone operation commands.
 */
static inline bool zbc_zone_count_supported(struct zbc_device_info *info)
{
	/*
	 * Assume that ZD/ZR devices support zone op counts.
	 * If this is a regular SMR device, check the support flag
	 * that is set during the scan.
	 */
	return zbc_device_is_zdr(info) ||
	       (info->zbd_flags & ZBC_ZONE_OP_COUNT_SUPPORT);
}

/**
 * @brief Convert LBA value to 512-bytes sector
 *
 * @return A number of 512B sectors.
 */
#define zbc_lba2sect(info, lba)	(((lba) * (info)->zbd_lblock_size) >> 9)

#define zbc_lba2sect_end(info, lba)	((((lba + 1) * (info)->zbd_lblock_size) >> 9) - 1)

/**
 * @brief Convert 512-bytes sector value to LBA
 *
 * @return A number of logical blocks.
 */
#define zbc_sect2lba(info, sect) (((sect) << 9) / (info)->zbd_lblock_size)

/**
 * @brief SCSI Sense keys definitions
 *
 * SCSI sense keys inspected in case of command error.
 */
enum zbc_sk {

	/** Not ready */
	ZBC_SK_NOT_READY	= 0x2,

	/** Medium error */
	ZBC_SK_MEDIUM_ERROR	= 0x3,

	/** Hardware Error */
	ZBC_SK_HARDWARE_ERROR	= 0x4,

	/** Illegal request */
	ZBC_SK_ILLEGAL_REQUEST	= 0x5,

	/** Data protect */
	ZBC_SK_DATA_PROTECT	= 0x7,

	/** Aborted command */
	ZBC_SK_ABORTED_COMMAND	= 0xB,
};

/**
 * @brief SCSI Additional sense codes and qualifiers definitions
 *
 * SCSI Additional sense codes and additional sense code qualifiers
 * inspected in case of command error.
 */
enum zbc_asc_ascq {

	/** Invalid field in CDB */
	ZBC_ASC_INVALID_FIELD_IN_CDB			= 0x2400,

	/** Logical block address out of range */
	ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE	= 0x2100,

	/** Unaligned write command */
	ZBC_ASC_UNALIGNED_WRITE_COMMAND			= 0x2104,

	/** write boundary violation */
	ZBC_ASC_WRITE_BOUNDARY_VIOLATION		= 0x2105,

	/** Attempt to read invalid data */
	ZBC_ASC_ATTEMPT_TO_READ_INVALID_DATA		= 0x2106,

	/** Read boundary violation */
	ZBC_ASC_READ_BOUNDARY_VIOLATION			= 0x2107,

	/** Zone is in the read-only condition */
	ZBC_ASC_ZONE_IS_READ_ONLY			= 0x2708,

	/** Zone is offline */
	ZBC_ASC_ZONE_IS_OFFLINE				= 0x2C0E,

	/** Insufficient zone resources */
	ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES		= 0x550E,

	/** Zone is inactive */
	ZBC_ASC_ZONE_IS_INACTIVE			= 0x2C12,

	/** Attempt to access GAP zone */
	ZBC_ASC_ATTEMPT_TO_ACCESS_GAP_ZONE		= 0x2109,

	/** Read error */
	ZBC_ASC_READ_ERROR				= 0x1100,

	/** Write error */
	ZBC_ASC_WRITE_ERROR				= 0x0C00,

	/** Format in progress */
	ZBC_ASC_FORMAT_IN_PROGRESS			= 0x0404,

	/** Parameter list length error */
	ZBC_ASC_PARAMETER_LIST_LENGTH_ERROR		= 0x1a00,

	/** Invalid field in parameter list */
	ZBC_ASC_INVALID_FIELD_IN_PARAMETER_LIST		= 0x2600,

	/** Internal target failure */
	ZBC_ASC_INTERNAL_TARGET_FAILURE			= 0x4400,

	/** Invalid command operation code */
	ZBC_ASC_INVALID_COMMAND_OPERATION_CODE		= 0x2000,

	/** Zone reset WP recommended */
	ZBC_ASC_ZONE_RESET_WP_RECOMMENDED		= 0x2A16,
};

/**
 * @brief Detailed error information data structure
 *
 * Standard and ZBC defined SCSI sense key and additional
 * sense codes are used to describe the error.
 *
 * Some commands return additional information identifying
 * the location of the failure.
 */
struct zbc_err_ext {

	/** Sense code */
	enum zbc_sk		sk;

	/** Additional sense code and sense code qualifier */
	enum zbc_asc_ascq	asc_ascq;

	/** Sense Data Information Field */
	unsigned long long	err_info;

	/** Sense Data Command Specific Information Field */
	unsigned long long	err_csinfo;

	/*** Conversion Boundary Failure field (48 bits) */
	uint64_t		err_cbf;

	/** Error information from ZONE ACTIVATE results header bytes 4-5 */
	uint16_t		err_za;
};

/**
 * @brief Get detailed error code of last operation
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] size	Number of bytes of error report to return
 * @param[out] err	Address where to return the error report
 *
 * Returns at the address specified by \a err a detailed error report
 * of the last command executed. If size >= sizeof(struct zbc_err) then
 * the entire zbc_err structure is returned; otherwise the first size
 * bytes are returned.
 *
 * For successful commands, all fields are set to 0.
 */
extern void zbc_errno_ext(struct zbc_device *dev,
			  struct zbc_err_ext *err, size_t size);

/* Legacy zbc_errno structure */
struct zbc_errno {
	enum zbc_sk             sk;
	enum zbc_asc_ascq       asc_ascq;
};

/**
 * @brief Get legacy error code of last operation
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[out] err	Address where to return the error report
 *
 * Returns at the address specified by \a err a detailed error report
 * of the last command executed. The error report is composed of the
 * SCSI sense key, sense code and sense code qualifier.
 * For successful commands, all three information are set to 0.
 *
 * The values returned here are the same as the first two fields
 * returned by zbc_errno_ext().
 */
extern void zbc_errno(struct zbc_device *dev, struct zbc_errno  *err);

/**
 * @brief Returns a string describing a sense key
 * @param[in] sk	Sense key
 *
 * @return A string describing a sense key.
 */
extern const char *zbc_sk_str(enum zbc_sk sk);

/**
 * @brief Returns a string describing a sense code
 * @param[in] asc_ascq	Sense code and sense code qualifier
 *
 * @return A string describing a sense code and sense code qualifier.
 */
extern const char *zbc_asc_ascq_str(enum zbc_asc_ascq asc_ascq);

/**
 * @brief return libzbc version as a string.
 *
 */
extern char const *zbc_version(void);

/**
 * @brief Test if a device is a zoned block device
 * @param[in] filename	Path to the device file
 * @param[in] unused	Previously used to allow emulation mode
 * @param[in] info	Address where to store the device information
 *
 * Test if a device supports the ZBC/ZAC command set.
 * If \a info is not NULL and the device is identified as a zoned
 * block device, the device information is returned at the address
 * specified by \a info.
 *
 * @return Returns a negative error code if the device test failed.
 * 1 is returned if the device is identified as a zoned zoned block device.
 * Otherwise, 0 is returned.
 */
extern int zbc_device_is_zoned(const char *filename, bool unused,
			       struct zbc_device_info *info);

/**
 * @brief ZBC device open flags
 *
 * These flags can be combined together and passed to \a zbc_open to change
 * that function default behavior. This is in particular useful for ATA devices
 * to force using the ATA backend driver to bypass any SAT layer that may
 * result in the SCSI backend driver being used.
 */
enum zbc_oflags {

	/*
	 * Block device backend is removed, keep zero mask
	 * defined for backwards compatibility.
	 */
	ZBC_O_DRV_BLOCK		= 0x00000000,

	/** Allow use of the SCSI backend driver */
	ZBC_O_DRV_SCSI		= 0x02000000,

	/** Allow use of the ATA backend driver */
	ZBC_O_DRV_ATA		= 0x04000000,

};

/**
 * @brief Open a ZBC device
 * @param[in] filename	Path to a device file
 * @param[in] flags	Device access mode flags
 * @param[out] dev	Opaque ZBC device handle
 *
 * Opens the device pointed by \a filename, and returns a handle to it
 * at the address specified by \a dev if the device is a zoned block device
 * supporting the ZBC or ZAC command set.
 * \a flags specifies the device access mode flags.O_RDONLY, O_WRONLY and O_RDWR
 * can be specified. Other POSIX defined O_xxx flags are ignored. Additionally,
 * if \a filename specifies the path to a zoned block device file or an emulated
 * device, O_DIRECT can also be specified (this is mandatory to avoid unaligned
 * write errors with zoned block device files). \a flags can also be or'ed with
 * one or more of the ZBC_O_DRV_xxx flags in order to restrict the possible
 * backend device drivers that libzbc will try when opening the device.
 *
 * @return If the device is not a zoned block device, -ENXIO will be returned.
 * Any other error code returned by open(2) can be returned as well.
 */
extern int zbc_open(const char *filename, int flags, struct zbc_device **dev);

/**
 * @brief Close a ZBC device
 * @param[in] dev	Device handle obtained with \a zbc_open
 *
 * Performs the equivalent to close(2) for a ZBC device open
 * using \a zbc_open.
 *
 * @return Can return any error that close(2) may return.
 */
extern int zbc_close(struct zbc_device *dev);

/**
 * @brief Get a ZBC device information
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] info	Address of the information structure to fill
 *
 * Get information about an open device. The \a info parameter is used to
 * return a device information. \a info must be allocated by the caller.
 */
extern void zbc_get_device_info(struct zbc_device *dev,
				struct zbc_device_info *info);

/**
 * @brief Print a device information
 * @param[in] info	The information to print
 * @param[in] out	File stream to print to
 *
 * Print the content of \a info to the file stream \a out.
 */
extern void zbc_print_device_info(struct zbc_device_info *info, FILE *out);

/**
 * @brief REPORT ZONES reporting options definitions
 *
 * Used to filter the zone information returned by the execution of a
 * REPORT ZONES command. Filtering is based on the value of the reporting
 * option and on the condition of the zones at the time of the execution of
 * the REPORT ZONES command.
 *
 * ZBC_RZ_RO_PARTIAL is not a filter: this reporting option can be combined
 * (or'ed) with any other filter option to limit the number of reported
 * zone information to the size of the REPORT ZONES command buffer.
 */
enum zbc_zone_reporting_options {

	/**
	 * List all of the zones in the device.
	 */
	ZBC_RZ_RO_ALL		= 0x00,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_EMPTY.
	 */
	ZBC_RZ_RO_EMPTY		= 0x01,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_IMP_OPEN.
	 */
	ZBC_RZ_RO_IMP_OPEN	= 0x02,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_EXP_OPEN.
	 */
	ZBC_RZ_RO_EXP_OPEN	= 0x03,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_CLOSED.
	 */
	ZBC_RZ_RO_CLOSED	= 0x04,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_FULL.
	 */
	ZBC_RZ_RO_FULL		= 0x05,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_RDONLY.
	 */
	ZBC_RZ_RO_RDONLY	= 0x06,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_OFFLINE.
	 */
	ZBC_RZ_RO_OFFLINE	= 0x07,

	/**
	 * List the zones with a Zone Condition of ZBC_ZC_INACTIVE.
	 */
	ZBC_RZ_RO_INACTIVE	= 0x08,

	/* 09h to 0Fh Reserved */

	/**
	 * List the zones with a zone attribute ZBC_ZA_RWP_RECOMMENDED set.
	 */
	ZBC_RZ_RO_RWP_RECMND	= 0x10,

	/**
	 * List the zones with a zone attribute ZBC_ZA_NON_SEQ set.
	 */
	ZBC_RZ_RO_NON_SEQ	= 0x11,

	/* 12h to 3Dh Reserved */

	/**
	 * List of the zones with a Zone Type of ZBC_ZT_GAP.
	 */
	ZBC_RZ_RO_GAP		= 0x3e,

	/**
	 * List of the zones with a Zone Condition of ZBC_ZC_NOT_WP.
	 */
	ZBC_RZ_RO_NOT_WP	= 0x3f,

	/**
	 * Partial report flag.
	 */
	ZBC_RZ_RO_PARTIAL	= 0x80,

};

/* Compatibility names from earlier version of libzbc */
#define zbc_reporting_options	zbc_zone_reporting_options
#define ZBC_RO_ALL		ZBC_RZ_RO_ALL
#define ZBC_RO_EMPTY		ZBC_RZ_RO_EMPTY
#define ZBC_RO_IMP_OPEN		ZBC_RZ_RO_IMP_OPEN
#define ZBC_RO_EXP_OPEN		ZBC_RZ_RO_EXP_OPEN
#define ZBC_RO_CLOSED		ZBC_RZ_RO_CLOSED
#define ZBC_RO_FULL		ZBC_RZ_RO_FULL
#define ZBC_RO_RDONLY		ZBC_RZ_RO_RDONLY
#define ZBC_RO_OFFLINE		ZBC_RZ_RO_OFFLINE
#define ZBC_RO_INACTIVE		ZBC_RZ_RO_INACTIVE
#define ZBC_RO_RWP_RECOMMENDED	ZBC_RZ_RO_RWP_RECMND
#define ZBC_RO_NON_SEQ		ZBC_RZ_RO_NON_SEQ
#define ZBC_RO_GAP		ZBC_RZ_RO_GAP
#define ZBC_RO_NOT_WP		ZBC_RZ_RO_NOT_WP
#define ZBC_RO_PARTIAL		ZBC_RZ_RO_PARTIAL

/**
 * @brief Get zone information
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	Sector from which to report zones
 * @param[in] ro	Reporting options
 * @param[in] zones	Pointer to the array of zone information to fill
 * @param[out] nr_zones	Number of zones in the array \a zones
 *
 * Get zone information matching the \a sector and \a ro arguments and
 * return the information obtained in the array \a zones and the number of
 * zone information obtained at the address specified by \a nr_zones.
 * The array \a zones must be allocated by the caller and \a nr_zones
 * must point to the size of the allocated array (number of zone information
 * structures in the array). The first zone reported will be the zone
 * containing or after \a sector.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
extern int zbc_report_zones(struct zbc_device *dev, uint64_t sector,
			    enum zbc_zone_reporting_options ro,
			    struct zbc_zone *zones, unsigned int *nr_zones);

/**
 * @brief Get the number of zones matches
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	Sector from which to report zones
 * @param[in] ro	Reporting options
 * @param[out] nr_zones	The number of matching zones
 *
 * Similar to \a zbc_report_zones, but returns only the number of zones that
 * \a zbc_report_zones would have returned. This is useful to determine the
 * total number of zones of a device to allocate an array of zone information
 * structures for use with \a zbc_report_zones.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_report_nr_zones(struct zbc_device *dev, uint64_t sector,
				      enum zbc_zone_reporting_options ro,
				      unsigned int *nr_zones)
{
	return zbc_report_zones(dev, sector, ro, NULL, nr_zones);
}

/**
 * @brief Get zone information
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	Sector from which to report zones
 * @param[in] ro	Reporting options
 * @param[out] zones	The array of zone information filled
 * @param[out] nr_zones	Number of zones in the array \a zones
 *
 * Similar to \a zbc_report_zones, but also allocates an appropriately sized
 * array of zone information structures and return the address of the array
 * at the address specified by \a zones. The size of the array allocated and
 * filled is returned at the address specified by \a nr_zones. Freeing of the
 * memory used by the array of zone information structures allocated by this
 * function is the responsibility of the caller.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 * Returns -ENOMEM if memory could not be allocated for \a zones.
 */
extern int zbc_list_zones(struct zbc_device *dev,
			  uint64_t sector, enum zbc_zone_reporting_options ro,
			  struct zbc_zone **zones, unsigned int *nr_zones);

/**
 * @brief Zone operation codes definitions
 *
 * Encode the operation to perform on a zone.
 */
enum zbc_zone_op {

	/**
	 * Reset zone write pointer.
	 */
	ZBC_OP_RESET_ZONE	= 0x01,

	/**
	 * Open a zone.
	 */
	ZBC_OP_OPEN_ZONE	= 0x02,

	/**
	 * Close a zone.
	 */
	ZBC_OP_CLOSE_ZONE	= 0x03,

	/**
	 * Finish a zone.
	 */
	ZBC_OP_FINISH_ZONE	= 0x04,

};

/**
 * @brief Zone operation flag definitions
 *
 * Control the behavior of zone operations.
 * Flags defined here can be or'ed together and passed to the functions
 * \a zbc_open_zone, \a zbc_close_zone, \a zbc_finish_zone and
 * \a link zbc_reset_zone.
 */
enum zbc_zone_op_flags {

	/**
	 * Operate on all possible zones.
	 */
	ZBC_OP_ALL_ZONES = 0x0000001,

};

/**
 * @brief Execute an operation on a zone
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	First sector of the target zone
 * @param[in] op	The operation to perform
 * @param[in] flags	Zone operation flags
 *
 * Execute an operation on the zone of \a dev starting at the sector
 * specified by \a sector. The target zone must be a write pointer zone,
 * that is, its type must be ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * The validity of the operation (reset, open, close or finish) depends on the
 * condition of the target zone. See \a zbc_reset_zone, \a zbc_open_zone,
 * \a zbc_close_zone and \a zbc_finish_zone for details.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and
 * the operation is executed on all possible zones.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
extern int zbc_zone_operation(struct zbc_device *dev, uint64_t sector,
			      enum zbc_zone_op op, unsigned int flags);

/**
 * @brief Execute an operation on a group of zones
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the first target zone
 * @param[in] count	The number of zones to process (0 still means one zone)
 * @param[in] op	The operation to perform
 * @param[in] flags	Zone operation flags
 *
 * Execute an operation on one or more zones of \a dev starting at the sector
 * specified by \a sector. The target zones must be write pointer zones,
 * that is, their type must be ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * The validity of the operation (reset, open, close or finish) depends on the
 * condition of the target group of zones. See \a zbc_reset_zone, \a zbc_open_zone,
 * \a zbc_close_zone and \a zbc_finish_zone for details.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and
 * the operation is executed on all possible zones.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
extern int zbc_zone_group_op(struct zbc_device *dev, uint64_t sector,
			     unsigned int count, enum zbc_zone_op op,
			     unsigned int flags);

/**
 * @brief Explicitly open a zone
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the zone to open
 * @param[in] flags	Zone operation flags
 *
 * Explicitly open the zone of \a dev starting at the sector specified by
 * \a sector. The target zone must be a write pointer zone, that is, its type
 * must be ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * The condition of the target zone must be ZBC_ZC_EMPTY, ZBC_ZC_IMP_OPEN or
 * ZBC_ZC_CLOSED. Otherwise, an error will be returned. Opening a zone with
 * the condition ZBC_ZC_EXP_OPEN has no effect (the zone condition is
 * unchanged).
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and
 * all possible zones that can be explicitly open will be (see ZBC/ZAC
 * specifications regarding the result of such operation).
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_open_zone(struct zbc_device *dev,
				uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_OPEN_ZONE, flags);
}

/**
 * @brief Explicitly open a group of zones
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the first zone to open
 * @param[in] count	The number of zones to open (0 still means one zone)
 * @param[in] flags	Zone operation flags
 *
 * Explicitly open \a count zones of \a dev starting at the sector specified by
 * \a sector. Target zones must be write pointer zones, that is, their type
 * must be ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * The condition of target zones must be ZBC_ZC_EMPTY, ZBC_ZC_IMP_OPEN or
 * ZBC_ZC_CLOSED. Otherwise, an error will be returned. Opening zones with
 * the condition ZBC_ZC_EXP_OPEN has no effect (the zone condition is
 * unchanged).
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and
 * all possible zones that can be explicitly open will be (see ZBC/ZAC
 * specifications regarding the result of such operation).
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_open_zones(struct zbc_device *dev, uint64_t sector,
				 unsigned int count, unsigned int flags)
{
	return zbc_zone_group_op(dev, sector, count,
				 ZBC_OP_OPEN_ZONE, flags);
}

/**
 * @brief Close an open zone
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the zone to close
 * @param[in] flags	Zone operation flags
 *
 * Close an implicitly or explicitly open zone. The zone to close is identified
 * by its first sector specified by \a sector. The target zone must be a write
 * pointer zone, that is, of type ZBC_ZT_SEQUENTIAL_REQ or
 * ZBC_ZT_SEQUENTIAL_PREF. Attempting to close a zone that is empty, full or
 * already closed will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and all
 * implicitly and explicitly open zones are closed.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_close_zone(struct zbc_device *dev,
				 uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_CLOSE_ZONE, flags);
}

/**
 * @brief Close a group of open zones
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the first zone to close
 * @param[in] count	The number of zones to close (0 still means one zone)
 * @param[in] flags	Zone operation flags
 *
 * Close an implicitly or explicitly open zone. The zone to close is identified
 * by its first sector specified by \a sector. The target zone must be a write
 * pointer zone, that is, of type ZBC_ZT_SEQUENTIAL_REQ or
 * ZBC_ZT_SEQUENTIAL_PREF. Attempting to close a zone that is empty, full or
 * already closed will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and all
 * implicitly and explicitly open zones are closed.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_close_zones(struct zbc_device *dev, uint64_t sector,
				  unsigned int count, unsigned int flags)
{
	return zbc_zone_group_op(dev, sector, count,
				 ZBC_OP_CLOSE_ZONE, flags);
}

/**
 * @brief Finish a write pointer zone
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the zone to finish
 * @param[in] flags	Zone operation flags
 *
 * Transition a write pointer zone to the full condition. The target zone is
 * identified by its first sector specified by \a sector. The target zone must
 * be a write pointer zone, that is, of type ZBC_ZT_SEQUENTIAL_REQ or
 * ZBC_ZT_SEQUENTIAL_PREF. Attempting to finish a zone that is already full
 * will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and all
 * implicitly and explicitly open zones as well as all closed zones are
 * transitioned to the full condition.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_finish_zone(struct zbc_device *dev,
				  uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_FINISH_ZONE, flags);
}

/**
 * @brief Finish a group of write pointer zones
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the zone to finish
 * @param[in] count	The number of zones to finish (0 still means one zone)
 * @param[in] flags	Zone operation flags
 *
 * Transition some (\a count) write pointer zones to the full condition.
 * The first target zone is identified by the first sector specified by \a sector.
 * Target zones must be write pointer zones, that is, of type ZBC_ZT_SEQUENTIAL_REQ
 * or ZBC_ZT_SEQUENTIAL_PREF. Attempting to finish zones that are already full will
 * succeed and the zone condition will remain unchanged. If ZBC_OP_ALL_ZONES is set
 * in \a flags then \a sector is ignored and all implicitly and explicitly open
 * zones as well as all closed zones are transitioned to the full condition.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_finish_zones(struct zbc_device *dev, uint64_t sector,
				   unsigned int count, unsigned int flags)
{
	return zbc_zone_group_op(dev, sector, count,
				 ZBC_OP_FINISH_ZONE, flags);
}

/**
 * @brief Reset the write pointer of a zone
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	The first sector of the zone to reset
 * @param[in] flags	Zone operation flags
 *
 * Resets the write pointer of the zone identified by its first sector
 * specified by \a sector. The target zone must be a write pointer zone,
 * that is, of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * Attempting to reset a write pointer zone that is already empty
 * will succeed and the zone condition will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and all
 * write pointer zones that are not empty will be reset.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_reset_zone(struct zbc_device *dev,
				 uint64_t sector, unsigned int flags)
{
	return zbc_zone_operation(dev, sector,
				  ZBC_OP_RESET_ZONE, flags);
}

/**
 * @brief Reset the write pointer of a group of zones
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] sector	First sector of the zone to reset
 * @param[in] count	The number of zones to reset (0 still means one zone)
 * @param[in] flags	Zone operation flags
 *
 * Resets write pointers of \a count of zones starting from the first sector
 * of the first zone specified by \a sector. Target zones must be write pointer
 * zones, that is, of type ZBC_ZT_SEQUENTIAL_REQ or ZBC_ZT_SEQUENTIAL_PREF.
 * Attempting to reset write pointer zones that are already empty will succeed
 * and the condition of the processed zones will remain unchanged.
 * If ZBC_OP_ALL_ZONES is set in \a flags then \a sector is ignored and all
 * write pointer zones that are not empty will be reset.
 *
 * Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_reset_zones(struct zbc_device *dev, uint64_t sector,
				  unsigned int count, unsigned int flags)
{
	return zbc_zone_group_op(dev, sector, count,
				 ZBC_OP_RESET_ZONE, flags);
}

/**
 * @brief REPORT ZONE DOMAINS reporting options definitions
 *
 * Used to filter the zone information returned by the execution of a
 * REPORT ZONE DOMAINS command.
 */
enum zbc_domain_report_options {

	/* Report all zone domains */
	ZBC_RZD_RO_ALL		= 0x00,

	/* Report all zone domains that for which all zones are active */
	ZBC_RZD_RO_ALL_ACTIVE	= 0x01,

	/* Report all zone domains that have active zones */
	ZBC_RZD_RO_ACTIVE	= 0x02,

	/* Report all zone domains that do not have any active zones */
	ZBC_RZD_RO_INACTIVE	= 0x03,
};

/**
 * @brief Get zone domain information
 * @param[in] dev	  Device handle obtained with \a zbc_open
 * @param[in] sector	  The starting sector for the report
 * @param[in] ro	  Domain reporting options
 * @param[in] domains	  Pointer to the array of domain descriptors to fill
 * @param[in] nr_domains  Number of domain descriptors in the array \a domains
 *
 * Get zone domain information from a Zone Domains device.
 * The \a domains array must be allocated by the caller and
 * \a nr_domains must contain the size of the allocated array (the number of
 * descriptors in the array). The number of returned domains may depend on the
 * chosen reporting options.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 *         Upon success, returns the total number of records that the device is
 *         reporting. This number may potentially exceed \a nr_domains. In this
 *         case, only \a nr_domains records in the input buffer are filled.
 */
extern int zbc_report_domains(struct zbc_device *dev, uint64_t sector,
			      enum zbc_domain_report_options ro,
			      struct zbc_zone_domain *domains,
			      unsigned int nr_domains);

/**
 * @brief List zone domain information
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] sector		The starting sector for report
 * @param[in] ro		Domain reporting options
 * @param[out] domains		Array of zone domain descriptors
 * @param[out] nr_domains	Number of domains in the array \a domains
 *
 * Similar to \a zbc_report_domains, but also allocates an appropriately sized
 * array of zone domain descriptors and returns the address of the array
 * at the address specified by \a domains. The size of the array allocated and
 * filled is returned at the address specified by \a nr_domains. Freeing of the
 * memory used by the array of domain descriptors allocated by this function
 * is the responsibility of the caller.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 * Returns -ENOMEM if memory could not be allocated for \a domains.
 */
extern int zbc_list_domains(struct zbc_device *dev, uint64_t sector,
			    enum zbc_domain_report_options ro,
			    struct zbc_zone_domain **pdomains,
			    unsigned int *pnr_domains);

/**
 * @brief REPORT REALMS reporting options definitions
 *
 * Used to filter the zone information returned by the execution of a
 * REPORT REALMS command.
 */
enum zbc_realm_report_options {

	/* Report all realms */
	ZBC_RR_RO_ALL		= 0x00,

	/* Report all realms that contain active SOBR zones */
	ZBC_RR_RO_SOBR		= 0x01,

	/* Report all realms that contain active SWR zones */
	ZBC_RR_RO_SWR		= 0x02,

	/* Report all realms that contain active SWP zones */
	ZBC_RR_RO_SWP		= 0x03,
};

/**
 * @brief Get zone realm information
 * @param[in] dev	  Device handle obtained with \a zbc_open
 * @param[in] ro	  Realm reporting options
 * @param[in] sector	  The starting sector of the first realm to report
 * @param[in] realms	  Pointer to the array of realm descriptors to fill
 * @param[out] nr_realms  Number of realm descriptors in the array \a realms
 *
 * Get zone realm information from an XMR device.
 * The \a realms array must be allocated by the caller and
 * \a nr_realms must point to the size of the allocated array (the number of
 * descriptors in the array). The number of returned realm records may depend
 * on the chosen reporting options.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
extern int zbc_report_realms(struct zbc_device *dev, uint64_t sector,
			     enum zbc_realm_report_options ro,
			     struct zbc_zone_realm *realms,
			     unsigned int *nr_realms);

/**
 * @brief Get the number of available zone realm descriptors.
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[out] nr_realms	The number of zone realms to be reported
 *
 * Similar to \a zbc_report_realms, but returns only the number of
 * zone realms that \a zbc_report_realms would have returned.
 * This is useful to determine the total number of realms of a device
 * to allocate an array of zone realm descriptors for use with
 * \a zbc_report_realms.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 */
static inline int zbc_report_nr_realms(struct zbc_device *dev,
				       unsigned int *nr_realms)
{
	return zbc_report_realms(dev, 0LL, ZBC_RR_RO_ALL, NULL, nr_realms);
}

/**
 * @brief List zone realm information
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] sector	  	The starting sector of the first realm to report
 * @param[in] ro		Realm reporting options
 * @param[out] prealms		Pointer to an array of zone realm descriptors
 * @param[out] pnr_realms	Points to the number of realms in \a realms
 *
 * Similar to \a zbc_report_realms, but also allocates an appropriately sized
 * array of zone realm descriptors and returns the address of the array
 * at the address specified by \a realms. The size of the array allocated and
 * filled is returned at the address specified by \a nr_realms. Freeing of the
 * memory used by the array of realm descriptors allocated by this function
 * is the responsibility of the caller.
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 * Returns -ENOMEM if memory could not be allocated for \a realms.
 */
extern int zbc_list_zone_realms(struct zbc_device *dev, uint64_t sector,
				enum zbc_realm_report_options ro,
				struct zbc_zone_realm **prealms,
				unsigned int *pnr_realms);

/**
 * @brief Activate the specified zones at a new zone domain
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] zsrc		If set, the nr_zones argument is valid
 * @param[in] all		If set, try to activate all zones
 * @param[in] use_32_byte_cdb	If true, use ZONE ACTIVATE(32)
 * @param[in] start_zone	512B sector of the first zone to activate
 * @param[in] nr_zones		The total number of zones to activate
 * @param[in] domain_id		Zone domain to activate
 * @param[out] actv_recs	Array of activation results records
 * @param[out] nr_actv_recs	The number of activation results records
 */
extern int zbc_zone_activate(struct zbc_device *dev, bool zsrc, bool all,
			     bool use_32_byte_cdb, uint64_t start_zone,
			     unsigned int nr_zones, unsigned int domain_id,
			     struct zbc_actv_res *actv_recs,
			     unsigned int *nr_actv_recs);

/**
 * @brief Query about possible results of zone activation
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] zsrc		If set, the nr_zones argument is valid
 * @param[in] all		If set, try to activate all zones
 * @param[in] use_32_byte_cdb	If true, use ZONE QUERY(32)
 * @param[in] start_zone	512B sector of the first zone to activate
 * @param[in] nr_zones		The total number of zones to activate
 * @param[in] domain_id		Zone domain to query about
 * @param[out] actv_recs	Array of activation results records
 * @param[out] nr_actv_recs	The number of activation results records
 */
extern int zbc_zone_query(struct zbc_device *dev, bool zsrc, bool all,
			  bool use_32_byte_cdb, uint64_t start_zone,
			  unsigned int nr_zones, unsigned int domain_id,
			  struct zbc_actv_res *actv_recs,
			  unsigned int *nr_actv_recs);

/**
 * @brief Return the expected number of activation records
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] zsrc		If set, the nr_zones argument is valid
 * @param[in] all		If set, try to activate all zones
 * @param[in] use_32_byte_cdb	If true, use 32-byte SCSI command
 * @param[in] start_zone	512B sector of the first zone to activate
 * @param[in] nr_zones		The total number of zones to activate
 * @param[in] domain_id		Zone domain to activate
 */
extern int zbc_get_nr_actv_records(struct zbc_device *dev, bool zsrc, bool all,
				   bool use_32_byte_cdb, uint64_t start_zone,
				   unsigned int nr_zones,
				   unsigned int domain_id);

/**
 * @brief Query about possible activation results of a number of zones
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] zsrc		If set, the nr_zones argument is valid
 * @param[in] all		If set, try to activate all zones
 * @param[in] use_32_byte_cdb	If true, use ZONE QUERY(32)
 * @param[in] start_zone	512B sector of the first zone to activate
 * @param[in] nr_zones		The total number of zones to activate
 * @param[in] domain_id		Zone domain to query about
 * @param[out] actv_recs	Points to the returned activation records
 * @param[out] nr_actv_recs	Number of returned activation results records
 */
extern int zbc_zone_query_list(struct zbc_device *dev, bool zsrc, bool all,
			       bool use_32_byte_cdb, uint64_t start_zone,
			       unsigned int nr_zones, unsigned int domain_id,
			       struct zbc_actv_res **pactv_recs,
			       unsigned int *pnr_actv_recs);
/**
 * @brief Read or change persistent XMR device settings
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] ctl		Contains all supported variables to control
 * @param[in] set		If false, just get the settings, otherwise set
 *
 * Typically, to set values, this function is called with \a set = false first
 * to get the current values, then the caller modifies the members of \a ctl
 * that need to be modified and then calls this function again with
 * \a set = true.
 */
extern int zbc_zone_activation_ctl(struct zbc_device *dev,
				   struct zbc_zd_dev_control *ctl, bool set);

/**
 * @brief Zoned Block Device Statistics
 *
 * This structure is filled with statistic counters that
 * are obtained by calling \a zbc_get_zbd_stats() function.
 */
struct zbc_zoned_blk_dev_stats {

	/** Maximum Open Zones */
	unsigned long long	max_open_zones;

	/** Maximum Explicitly Open SWR and SWP Zones */
	unsigned long long	max_exp_open_seq_zones;

	/** Maximum Implicitly Open SWR and SWP Zones */
	unsigned long long	max_imp_open_seq_zones;

	/** Maximum Implicitly Open SOBR Zones */
	unsigned long long	max_imp_open_sobr_zones;

	/** Minimum Empty Zones */
	unsigned long long	min_empty_zones;

	/** Zones Emptied */
	unsigned long long	zones_emptied;

	/** Maximum Non-sequential Zones */
	unsigned long long	max_non_seq_zones;

	/** Suboptimal Write Commands */
	unsigned long long	subopt_write_cmds;

	/** Commands Exceeding Optimal Limit */
	unsigned long long	cmds_above_opt_lim;

	/** Failed Explicit Opens */
	unsigned long long	failed_exp_opens;

	/** Read Rule Violations */
	unsigned long long	read_rule_fails;

	/** Write Rule Violations */
	unsigned long long	write_rule_fails;
};

/**
 * @brief Get Zoned Block Device statistics
 *
 * @param[in] dev		Device handle obtained with \a zbc_open
 * @param[in] stats		Points to \a zbc_zoned_blk_dev_stats
 *                              structure allocated by the caller
 *
 * @return Returns -EIO if an error happened when communicating with the device.
 * Returns -ENXIO if the device or the driver doesn't support ZBD statistics.
 */
extern int zbc_get_zbd_stats(struct zbc_device *dev,
			     struct zbc_zoned_blk_dev_stats *stats);

/**
 * @brief Read sectors from a device
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] buf	Caller supplied buffer to read into
 * @param[in] count	Number of 512B sectors to read
 * @param[in] offset	Offset where to start reading (512B sector unit)
 *
 * This is the equivalent of the standard system call pread(2) that
 * operates on a ZBC device handle and uses 512B sector unit addressing
 * for the amount of data and the position on the device of the data to read.
 * Attempting to read data across zone boundaries or after the write pointer
 * position of a write pointer zone is possible only if the device allows
 * unrestricted reads. This is indicated by the device information structure
 * flags field, using the flag ZBC_UNRESTRICTED_READ.
 * The range of 512B sectors to read, starting at \a offset and spanning
 * \a count 512B sectors must be aligned on logical blocks boundaries.
 * That is, for a 4K logical block size device, \a count and \a offset
 * must be multiples of 8.
 *
 * @return Any error returned by pread(2) can be returned. On success,
 * the number of 512B sectors read is returned.
 */
extern ssize_t zbc_pread(struct zbc_device *dev, void *buf,
			 size_t count, uint64_t offset);

/**
 * @brief Write sectors to a device
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] buf	Caller supplied buffer to write from
 * @param[in] count	Number of 512B sectors to write
 * @param[in] offset	Offset where to start writing (512B sector unit)
 *
 * This is the equivalent of the standard system call pwrite(2) that
 * operates on a ZBC device handle, and uses 512B sector unit addressing
 * for the amount of data and the position on the device of the data to
 * write. On a host-aware device, any range of 512B sector is acceptable.
 * On a host-managed device, the range os sectors to write can span several
 * conventional zones but cannot span conventional and sequential write
 * required zones. When writing to a sequential write required zone, \a offset
 * must specify the current write pointer position of the zone.
 * The range of 512B sectors to write, starting at \a offset and spanning
 * \a count 512B sectors must be aligned on physical blocks boundaries.
 * That is, for a 4K physical block size device, \a count and \a offset
 * must be multiples of 8.
 *
 * @return Any error returned by write(2) can be returned. On success,
 * the number of logical blocks written is returned.
 */
extern ssize_t zbc_pwrite(struct zbc_device *dev, const void *buf,
			  size_t count, uint64_t offset);

/**
 * @brief Read sectors from a device using multiple buffers
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] iov	Caller supplied read buffers to read into.
 *			Read buffer length is specified in 512B sectors
 * @param[in] iovcnt	Number of \a iov buffers
 * @param[in] offset	Offset where to start reading (512B sector unit)
 *
 * This is the equivalent of the standard system call preadv(2) and
 * behaves otherwise as described in \a zbc_pread. The call reads
 * \a iovcnt buffers from the file associated with \a zbc_open into
 * the buffers described by \a iov ("scatter input").
 */
extern ssize_t zbc_preadv(struct zbc_device *dev,
			  const struct iovec *iov, int iovcnt,
			  uint64_t offset);

/**
 * @brief Write sectors to a device usig multiple buffers
 * @param[in] dev	Device handle obtained with \a zbc_open
 * @param[in] iov	Caller supplied write buffers to write from.
 *			Write buffer length is specified in 512B sectors
 * @param[in] iovcnt	Number of \a iov buffers
 * @param[in] offset	Offset where to start writing (512B sector unit)
 *
 * This is the equivalent of the standard system call pwritev(2) and
 * behaves otherwise as described in \a zbc_pwrite. The call writes
 * \a iovcnt buffers from the file associated with \a zbc_open from
 * the buffers described by \a iov ("gather output").
 */
extern ssize_t zbc_pwritev(struct zbc_device *dev,
			   const struct iovec *iov, int iovcnt,
			   uint64_t offset);

/**
 * @brief Map a buffer to an I/O vector
 * @param[in] buf	Data buffer to map
 * @param[in] sectors	Size of \a buf in 512B sectors unit
 * @param[in] iov	Array of I/O vectors to which \a buf will be mapped
 * @param[in] iovcnt	The maximum number of entries in the \a iov array
 * @param[in] iovlen	The maximum size of a single I/O vector entry in 512B
 * 			sectors.
 *
 * Map the buffer \a buf to a set of I/O vectors of size \a iovlen.
 * This is a utility function that is called by libzbc tools and test
 * programs. It can be handy to library users as well.
 *
 * @return the number of I/O vectors mapped or a negative error code
 * in case of error
 */
extern int zbc_map_iov(const void *buf, size_t sectors,
		       struct iovec *iov, int iovcnt, size_t iovlen);

/**
 * @brief Flush a device write cache
 * @param[in] dev	Device handle obtained with \a zbc_open
 *
 * This is the equivalent to fsync/fdatasunc but it operates at the
 * device cache level.
 *
 * @return Returns 0 on success and -EIO in case of error.
 */
extern int zbc_flush(struct zbc_device *dev);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _LIBZBC_H_ */
