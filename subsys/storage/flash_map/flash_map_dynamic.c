/*
 * Copyright (c) 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <storage/flash_map_dynamic.h>
#include <storage/flash_map.h>

#include "settings/settings.h"
#include <errno.h>

struct direct_immediate_value {
	size_t len;
	void *dest;
	uint8_t fetched;
};

static int nvs_load_callback(const char *name, size_t len,
					 settings_read_cb read_cb, void *cb_arg,
					 void *param)
{
	const char *next;
	size_t name_len;
	int rc;
	struct direct_immediate_value *one_value =
					(struct direct_immediate_value *)param;

	name_len = settings_name_next(name, &next);
	if (name_len != 0)
		return 0;

	if (len == one_value->len) {
		rc = read_cb(cb_arg, one_value->dest, len);
		if (rc >= 0) {
			one_value->fetched = 1;
			return 0;
		}

		return rc;
	}

	return -EINVAL;
}

static int read_from_nvs(const char *name, void *dest, size_t len)
{
	int rc;
	struct direct_immediate_value dov;

	dov.fetched = 0;
	dov.len = len;
	dov.dest = dest;

	rc = settings_load_subtree_direct(name, nvs_load_callback, (void *)&dov);
	if (rc != 0)
		return rc;

	if (dov.fetched == 0) /* No error, but nothing fetched.. */
		return ENOENT;

	return 0;
}

static int set_partition_cnt(uint8_t cnt)
{
	return settings_save_one(PARTITION_CNT, (const void *)&cnt, sizeof(uint8_t));
}

int get_partition_cnt(uint8_t *cnt)
{
	return read_from_nvs(PARTITION_CNT, (void *) cnt, sizeof(uint8_t));
}

static int get_partition_from_settings(uint8_t no, struct flash_partition_info *partition)
{
	int rc;
	char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;

	partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(no);
	rc = read_from_nvs(partition_no, (void *) partition, sizeof(struct flash_partition_info));

	return rc;
}

/* Gets the respective "partitions/${no}" instance. */
int get_partition_at_index(uint8_t no, struct flash_partition_info *partition)
{
	int rc;
	uint8_t cnt;

	rc = get_partition_cnt(&cnt);
	if (rc != 0)
		return rc;

	if (cnt < no)
		return -ENOENT;

	return get_partition_from_settings(no, partition);
}

/* Goes through all the available partitions and returns the partition for which
 * partition->fa_id = id */
int get_partition_by_id(int id, struct flash_partition_info *partition)
{
	int rc, i;
	uint8_t cnt;
	struct flash_partition_info tmp;

	rc = get_partition_cnt(&cnt);
	if (rc != 0)
		return rc;

	for (i = 0; i < cnt; i ++)
	{
		rc = get_partition_from_settings(i, &tmp);
		if (rc != 0)
			continue; /* No matter what, go through all iterations until cnt. */

		if (tmp.fa_id == id)
		{
			partition->fa_id   = tmp.fa_id;
			partition->fa_off  = tmp.fa_off;
			partition->fa_size = tmp.fa_size;

			return 0;
		}
	}

	return -ENOENT;
}

/* This function writes the partition number required by the user (specified by
 * the first argument). Valid options are 0-9, and we will write the partition
 * info at "partitions/${no}. If there's something already at this entry, we
 * will overwrite it with the new contents. If there's nothing at this partition
 * entry, we exit with error. */
int add_partition_at_index(uint8_t no, struct flash_partition_info *partition)
{
	int rc;
	char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;
	struct flash_partition_info tmp;

	rc = get_partition_at_index(no, &tmp);
	if (rc != 0)
		return -EINVAL; /* Partition doesn't exist, Can't overwrite it. */

	partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(no);
	rc = settings_save_one(partition_no, (const void *) partition,
			sizeof(struct flash_partition_info));

	return rc;
}

/* This function goes through the complete list of available partitions and overrides the
 * partition for which id matches with partition->fa_id. Exits with error if no such
 * partition is found. The user would be expected to add a partition using add_partition_at_end
 * in such a case. */
int add_partition_by_id(uint8_t id, struct flash_partition_info *partition)
{
	int rc, i;
	uint8_t cnt;
	char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;
	struct flash_partition_info tmp;

	rc = get_partition_cnt(&cnt);
	if (rc != 0)
		return rc;

	for (i = 0; i < cnt; i ++)
	{
		rc = get_partition_at_index(i, &tmp);
		if (rc != 0)
			continue;

		if (tmp.fa_id == id)
		{
			partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(i);
			rc = settings_save_one(partition_no, (const void *) partition,
					sizeof(struct flash_partition_info));

			return rc;
		}
	}

	return -ENOENT;
}

/* This function adds a new partition to the system at get_partition_cnt+1. */
int add_partition_at_end(struct flash_partition_info *partition)
{
	int rc;
	uint8_t cnt;
	char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;

	rc = get_partition_cnt(&cnt);
	if (rc != 0) /* Possible that this hasn't been written yet.. */
	{
		rc = set_partition_cnt(0);
		if (rc != 0)
			return rc;

		/* Try to re-read the count (if it fails, exit). */
		rc = get_partition_cnt(&cnt);
		if (rc != 0)
			return rc;
	}

	if (cnt >= CONFIG_MAX_DYNAMIC_PARTITIONS)
		return -ENOENT;

	partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(cnt);
	rc = settings_save_one(partition_no, (const void *) partition,
			sizeof(struct flash_partition_info));
	if (rc != 0)
		return rc;

	cnt ++;
	return set_partition_cnt(cnt);
}
