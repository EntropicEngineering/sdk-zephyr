/*
 * Copyright (c) 2021 Entropic Engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <storage/flash_map_dynamic.h>
#include <storage/flash_map.h>

#include <settings/settings.h>
#include <errno.h>
#include <autoconf.h>

struct direct_immediate_value {
    size_t len;
    void *dest;
    uint8_t fetched;
};

static int nvs_load_callback(char const *name, size_t len,
                             settings_read_cb read_cb, void *cb_arg,
                             void *param) {
    const char *next;
    size_t name_len;
    int rc;
    struct direct_immediate_value *one_value =
        (struct direct_immediate_value *) param;

    name_len = settings_name_next(name, &next);
    if (name_len != 0) return 0;

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

static int read_from_nvs(char const *name, void *dest, size_t len) {
    int rc;
    struct direct_immediate_value div;

    div.fetched = 0;
    div.len = len;
    div.dest = dest;

    rc = settings_load_subtree_direct(name, nvs_load_callback, (void *) &div);
    if (rc != 0) return rc;

    if (div.fetched == 0) {
        /* No error, but nothing fetched. */
        return -ENOENT;
    }

    return 0;
}

static int save_partition_count_to_settings(uint8_t cnt) {
    return settings_save_one(PARTITION_COUNT, (void const *) &cnt, sizeof(uint8_t));
}

static int save_partition_to_settings(uint8_t no, struct flash_partition_info *partition) {
    int rc;
    char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;

    partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(no);
    rc = settings_save_one(partition_no, (void const *) partition, sizeof(struct flash_partition_info));

    return rc;
}

static int get_partition_from_settings(uint8_t no, struct flash_partition_info *partition) {
    int rc;
    char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;

    partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(no);
    rc = read_from_nvs(partition_no, (void *) partition, sizeof(struct flash_partition_info));

    return rc;
}

int get_dynamic_partition_count(uint8_t *cnt) {
    int rc;
    rc = read_from_nvs(PARTITION_COUNT, (void *) cnt, sizeof(uint8_t));
    if (rc == -ENOENT) {    // No partition count in settings
        *cnt = 0;
        return 0;
    }
    return rc;
}

/* Gets the respective "partitions/${no}" instance. */
int get_partition_at_index(uint8_t no, struct flash_partition_info *partition) {
    int rc;
    uint8_t cnt;

    rc = get_dynamic_partition_count(&cnt);
    if (rc != 0) return rc;

    if (cnt < no) return -ENOENT;

    return get_partition_from_settings(no, partition);
}

/* Goes through all the available partitions and returns the partition for which
 * partition->fa_id = id */
int get_partition_by_id(int id, struct flash_partition_info *partition) {
    int rc, i;
    uint8_t cnt;
    struct flash_partition_info tmp;

    rc = get_dynamic_partition_count(&cnt);
    if (rc != 0) return rc;

    for (i = 0; i < cnt; i++) {
        rc = get_partition_from_settings(i, &tmp);
        if (rc != 0) continue; /* No matter what, go through all iterations until cnt. */

        if (tmp.fa_id == id) {
            partition->fa_id = tmp.fa_id;
            partition->fa_off = tmp.fa_off;
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
int set_partition_at_index(uint8_t no, struct flash_partition_info *partition) {
    int rc;
    struct flash_partition_info tmp;

    rc = get_partition_at_index(no, &tmp);
    if (rc != 0) return -EINVAL; /* Partition doesn't exist, Can't overwrite it. */

    return save_partition_to_settings(no, partition);
}

/* This function goes through the complete list of available partitions and overrides the
 * partition for which id matches with partition->fa_id. Exits with error if no such
 * partition is found. The user would be expected to add a partition using add_dynamic_partition
 * in such a case. */
int set_partition_by_id(uint8_t id, struct flash_partition_info *partition) {
    int rc, i;
    int hole = -1;
    uint8_t cnt;
    struct flash_partition_info tmp;

    rc = get_dynamic_partition_count(&cnt);
    if (rc != 0) return rc;

    for (i = 0; i < cnt; i++) {
        rc = get_partition_from_settings(i, &tmp);
        if (rc == -ENOENT) {
            /* Index lower than count, but not in settings */
            hole = i;
            /* Continue through count */
            continue;
        } else if (rc != 0) {
            /* Settings error */
            return rc;
        };

        if (tmp.fa_id == id) {
            return save_partition_to_settings(i, partition);
        }
    }

    /* id not found in partitions */

    if (hole >= 0) {
        /* Use deleted partition index */
        return save_partition_to_settings(hole, partition);
    }

    return add_dynamic_partition(partition);
}

/* This function adds a new partition to the system at get_dynamic_partition_count+1. */
int add_dynamic_partition(struct flash_partition_info *partition) {
    int rc;
    uint8_t cnt;

    rc = get_dynamic_partition_count(&cnt);
    if (rc != 0) return rc;

    if (cnt >= CONFIG_MAX_DYNAMIC_PARTITIONS) return -ENOENT;

    /* Write count first to ensure partition info is never lost. *
     * Empty partition indices are filled. */
    rc = save_partition_count_to_settings(cnt + 1);
    if (rc != 0) return rc;

    return save_partition_to_settings(cnt, partition);
}
