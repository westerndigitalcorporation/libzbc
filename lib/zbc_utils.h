/* SPDX-License-Identifier: BSD-2-Clause */
/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2021, Western Digital or its Affiliates. All rights reserved.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#ifndef __LIBZBC_UTILS_H__
#define __LIBZBC_UTILS_H__

/**
 * Get a sysfs file integer value.
 */
int zbc_get_sysfs_val_ull(const char *sysfs_path, unsigned long long *val);

/**
 * Get a sysfs file string value.
 */
int zbc_get_sysfs_val_str(const char *sysfs_path, char *str, int max_len);


/**
 * Get a block device sysfs queue integer attribute.
 */
int zbc_get_sysfs_queue_val_ull(char *dev_name, const char *attr,
				unsigned long long *val);

/**
 * Get a block device sysfs queue string attribute.
 */
int zbc_get_sysfs_queue_str(char *dev_name, const char *attr,
			    char *str, int max_len);

/**
 * Get a block device sysfs device integer attribute.
 */
int zbc_get_sysfs_device_val_ull(char *dev_name, const char *attr,
				 unsigned long long *val);

/**
 * Get a block device sysfs device string attribute.
 */
int zbc_get_sysfs_device_str(char *dev_name, const char *attr,
			     char *str, int max_len);

#endif /* __LIBZBC_UTILS_H__ */

