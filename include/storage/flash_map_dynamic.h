/*
 * Copyright (c) 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_
#define ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Replica of "struct flash_area" (flash_map.h) except for fa_device_id / fa_dev_name
 * which are placed in the "map_info" once. */
struct flash_partition_info {
	uint8_t  fa_id;        /* ID number */
	uint8_t  pad[3];	   /* sizeof(struct map_parition_info) = 12. */
	off_t    fa_off;       /* Start offset from the beginning of the flash device */
	size_t   fa_size;      /* Total size */
};

/* Settings "Keys" */
#define PARTITION_CNT "partitions/cnt"
#define PARTITION_DEV_NAME "partitions/dev_name"
#define PARTITION_NO "partitions/#" /* Support up to 10 partitions (0-9). */

#define INT_TO_CHAR(x) (x + '0')
#define MAX_PARTITIONS 10

/* Hardcoded fa_id for user. */
#define IMAGE_0_PARTITION_ID FLASH_AREA_ID(image_0)
#define IMAGE_1_PARTITION_ID FLASH_AREA_ID(image_1)
#define STORAGE_PARTITION_ID FLASH_AREA_ID(storage)

int add_partition_at_index(uint8_t no, struct flash_partition_info *partition);
int add_partition_by_id(uint8_t id, struct flash_partition_info *partition);
int add_partition_at_end(struct flash_partition_info *partition);

int get_partition_at_index(uint8_t no, struct flash_partition_info *partition);
int get_partition_by_id(int id, struct flash_partition_info *partition);

int get_partition_cnt(uint8_t *cnt);

#endif /* ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_ */
