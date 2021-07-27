// SPDX-License-Identifier: BSD-2-Clause
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2021, Western Digital. All rights reserved.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>

#include "zbc_utils.h"

/**
 * Get a sysfs file integer value.
 */
int zbc_get_sysfs_val_ull(const char *sysfs_path, unsigned long long *val)
{
	FILE *f;
	int ret;

	f = fopen(sysfs_path, "r");
	if (!f)
		return -1;

	ret = fscanf(f, "%llu", val);

	fclose(f);
	if (ret != 1)
		return 0;

	return 0;
}

static int zbc_read_str(FILE *file, char *buf, int buf_size)
{
	int len;

	memset(buf, 0, buf_size);

	if (!fgets(buf, buf_size, file))
		return 0;

	/* Clear any trailing white space, tab and carriage return */
	len = strlen(buf) - 1;
	while (len > 0) {
		if (buf[len] == ' ' ||
		    buf[len] == '\t' ||
		    buf[len] == '\r' ||
		    buf[len] == '\n') {
			buf[len] = '\0';
			len--;
		} else {
			break;
		}
	}

	return len;
}

/**
 * Get a sysfs file string value.
 */
int zbc_get_sysfs_val_str(const char *sysfs_path, char *str,
			  int max_len)
{
	FILE *f;
	int ret;

	f = fopen(sysfs_path, "r");
	if (!f)
		return -1;

	ret = zbc_read_str(f, str, max_len);

	fclose(f);

	if (!ret)
		return -1;

	return 0;
}

static char *zbc_sysfs_path(char *dev_name,
			    const char *group, const char *attr)
{
	char *path;
	int ret;

	ret = asprintf(&path, "/sys/block/%s/%s/%s",
		       basename(dev_name), group, attr);
	if (ret < 0)
		return NULL;

	return path;
}

static int zbc_get_sysfs_group_val_ull(char *dev_name,
				       const char *group, const char *attr,
				       unsigned long long *val)
{
	char *path = zbc_sysfs_path(dev_name, group, attr);
	int ret = -1;

	if (path) {
		ret = zbc_get_sysfs_val_ull(path, val);
		free(path);
	}

	return ret;
}

static int zbc_get_sysfs_group_val_str(char *dev_name,
				       const char *group, const char *attr,
				       char *str, int max_len)
{
	char *path = zbc_sysfs_path(dev_name, group, attr);
	int ret = -1;

	if (path) {
		ret = zbc_get_sysfs_val_str(path, str, max_len);
		free(path);
	}

	return ret;
}

/**
 * Get a block device sysfs queue integer attribute.
 */
int zbc_get_sysfs_queue_val_ull(char *dev_name, const char *attr,
				unsigned long long *val)
{
	return zbc_get_sysfs_group_val_ull(dev_name, "queue", attr, val);
}

/**
 * Get a block device sysfs queue string attribute.
 */
int zbc_get_sysfs_queue_str(char *dev_name, const char *attr,
			    char *str, int max_len)
{
	return zbc_get_sysfs_group_val_str(dev_name, "queue", attr,
					   str, max_len);
}

/**
 * Get a block device sysfs device integer attribute.
 */
int zbc_get_sysfs_device_val_ull(char *dev_name, const char *attr,
				 unsigned long long *val)
{
	return zbc_get_sysfs_group_val_ull(dev_name, "device", attr, val);
}

/**
 * Get a block device sysfs device string attribute.
 */
int zbc_get_sysfs_device_str(char *dev_name, const char *attr,
			     char *str, int max_len)
{
	return zbc_get_sysfs_group_val_str(dev_name, "device", attr,
					   str, max_len);
}
