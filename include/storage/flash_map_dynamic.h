/*
 * Copyright (c) 2021 Entropic Engineering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_
#define ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_

#include <storage/flash_map.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Settings "Keys" */
#define PARTITION_COUNT "partitions/cnt"
#define PARTITION_DEV_NAME "partitions/dev_name"
#define PARTITION_NO "partitions/#" /* Support up to 10 partitions (0-9). */

#define INT_TO_CHAR(x) (x + '0')

/* Hardcoded fa_id for user. */
#define IMAGE_0_PARTITION_ID FLASH_AREA_ID(image_0)
#define IMAGE_1_PARTITION_ID FLASH_AREA_ID(image_1)
#define STORAGE_PARTITION_ID FLASH_AREA_ID(storage)

/**
 * Array provides global pointers to data derived from flash.
 */
extern struct flash_area const dynamic_flash_map[CONFIG_MAX_DYNAMIC_PARTITIONS];
extern uint8_t const dynamic_flash_map_entries;

int set_partition_at_index(uint8_t no, struct flash_area *partition);
int set_partition_by_id(struct flash_area *partition);
int add_dynamic_partition(struct flash_area *partition);

#endif /* ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_ */
