/*
 * Copyright (c) 2021 Entropic Engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <storage/flash_map_dynamic.h>
#include <storage/flash_map.h>

#include <settings/settings.h>
#include <errno.h>
#include <init.h>

#include <autoconf.h>
#include <assert.h>
#include <string.h>

extern const struct flash_area *flash_map;
extern const int flash_map_entries;

/* Array provides global pointers to data derived from settings. */
struct flash_area const dynamic_flash_map[CONFIG_MAX_DYNAMIC_PARTITIONS] = {0};
uint8_t const dynamic_flash_map_entries = 0;
/* Enable local access to data. */
static uint8_t *p_dynamic_flash_map_entries = (uint8_t *) &dynamic_flash_map_entries;

/*
 * Near replica of "struct flash_area" except for fa_device_id & fa_dev_name:
 * fa_device_id is replaced by an indicator as to whether the data has been erased.
 * fa_dev_name is omitted;
 * Correct values for both fa_device_id & fa_dev_name are inserted into dynamic_flash_map.
 */
struct flash_partition_info {
    uint8_t  fa_id;         /* ID number */
    uint8_t  empty;         /* Test against flash_area_erase_val */
    uint16_t pad16;
    off_t    fa_off;        /* Start offset from the beginning of the flash device */
    size_t   fa_size;       /* Total size */
};

struct direct_immediate_value {
    size_t len;
    void *dest;
    uint8_t fetched;
};

static uint8_t fa_device_id;
static const char *fa_dev_name;

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

static int save_partition_count_to_settings(uint8_t count) {
    int rc;

    rc = settings_save_one(PARTITION_COUNT, (void const *) &count, sizeof(uint8_t));

    if (rc == 0) {
        *p_dynamic_flash_map_entries = count;
    }

    return rc;
}

static int save_partition_to_settings(uint8_t no, struct flash_area *partition) {
    int rc;
    char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;
    struct flash_partition_info info = *(struct flash_partition_info *) partition;

    partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(no);
    rc = settings_save_one(partition_no, (void const *) &info, sizeof(struct flash_partition_info));

    if (rc == 0) {
        memcpy((void *) &dynamic_flash_map[no], partition, sizeof(struct flash_area));
    }

    return rc;
}

static int get_partition_from_settings(uint8_t no, struct flash_area *partition) {
    int rc;
    char partition_no[sizeof(PARTITION_NO)] = PARTITION_NO;

    partition_no[sizeof(PARTITION_NO) - 2] = INT_TO_CHAR(no);
    rc = read_from_nvs(partition_no, (void *) partition, sizeof(struct flash_partition_info));
    if (rc != 0) return rc;

    partition->fa_device_id = fa_device_id;
    partition->fa_dev_name = fa_dev_name;

    return 0;
}

static int get_dynamic_partition_count(uint8_t *cnt) {
    int rc;
    rc = read_from_nvs(PARTITION_COUNT, (void *) cnt, sizeof(uint8_t));
    if (rc == -ENOENT) {    // No partition count in settings
        *cnt = 0;
        return 0;
    }
    return rc;
}

/* Gets the respective "partitions/${no}" instance. */
static int get_partition_at_index(uint8_t no, struct flash_area *partition) {

    if (no >= dynamic_flash_map_entries) return -ENOENT;

    return get_partition_from_settings(no, partition);
}

/* This function writes the partition number required by the user (specified by
 * the first argument). Valid options are 0-9, and we will write the partition
 * info at "partitions/${no}. If there's something already at this entry, we
 * will overwrite it with the new contents. If there's nothing at this partition
 * entry, we exit with error. */
int set_partition_at_index(uint8_t no, struct flash_area *partition) {
    int rc;
    struct flash_area tmp;

    rc = get_partition_at_index(no, &tmp);
    if (rc != 0) return -EINVAL; /* Partition doesn't exist, Can't overwrite it. */

    return save_partition_to_settings(no, partition);
}

/* This function goes through the complete list of available partitions and overrides the
 * partition for which id matches with partition->fa_id. If no partition is found with a
 * matching id, a new entry is added.
 */
int set_partition_by_id(struct flash_area *partition) {
    int rc;
    int hole = -1;
    struct flash_area tmp;

    for (int i = 0; i < dynamic_flash_map_entries; i++) {
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

        if (tmp.fa_id == partition->fa_id) {
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
int add_dynamic_partition(struct flash_area *partition) {
    int rc;
    uint8_t count;

    /* Check flash in case dynamic_flash_map_entries is somehow inaccurate. */
    rc = get_dynamic_partition_count(&count);
    if (rc != 0) return rc;

    if (count >= CONFIG_MAX_DYNAMIC_PARTITIONS) return -ENOENT;

    /* Write count first to ensure partition info is never lost. *
     * Empty partition indices are filled. */
    rc = save_partition_count_to_settings(count + 1);
    if (rc != 0) return rc;

    return save_partition_to_settings(count, partition);
}

static int initialize_dynamic_flash_map(const struct device *dev)
{
    ARG_UNUSED(dev);
    int rc;

    fa_device_id = flash_map[0].fa_device_id;
    fa_dev_name = flash_map[0].fa_dev_name;

    rc = settings_subsys_init();;
    if (rc != 0) return rc;

    rc = get_dynamic_partition_count(p_dynamic_flash_map_entries);
    if (rc != 0) return rc;

    /*
     * Number of dynamic entries must be one less than flash map entries
     * because storage partition must remain static.
     */
    assert(dynamic_flash_map_entries < flash_map_entries);

    for (int i = 0; i < dynamic_flash_map_entries; i++) {
        rc = get_partition_from_settings(i, (struct flash_area *) &dynamic_flash_map[i]);
        if (rc == -ENOENT) {
            /* Deleted entry, ignore */
            continue;
        } else if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

SYS_INIT(initialize_dynamic_flash_map, APPLICATION,CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
