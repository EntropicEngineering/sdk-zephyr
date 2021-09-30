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

struct partition_header_info {
	uint8_t  flash_dev_id;
	uint8_t  pad[3]; /* sizeof (struct map_header_info) = 36 */
	char     flash_dev_name[32];
};

/* Replica of "struct flash_area" (flash_map.h) except for fa_device_id / fa_dev_name
 * which are placed in the "map_info" once. */
struct flash_partition_info {
	uint8_t  fa_id;        /* ID number */
	uint8_t  pad[3];	   /* sizeof(struct map_parition_info) = 12. */
	off_t    fa_off;       /* Start offset from the beginning of the flash device */
	size_t   fa_size;      /* Total size */
};

/* The user is expected to firslty "create" a table and then add individual partitions
 * to it. */
int create_partition_table(struct partition_header_info *info);
int add_partition(struct flash_partition_info *partition);
int get_partition(struct flash_partition_info *partition, int part_no);

/* Delete the currently written partition table completely. */
int delete_partition_table(void);

#endif /* ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_DYNAMIC_H_ */
