/*
 * Copyright (c) 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <storage/flash_map_dynamic.h>
#include <storage/flash_map.h>

struct flash_map_info {
	uint32_t map_magic; /* Magic (0xDEADBEEF) to tell if the partition map is valid or not. */
	uint8_t  partition_cnt;
	uint8_t  pad[3]; /* sizeof(struct map_info) = 8. */
};

/* First 64 bytes are reserved to keep the header info. */
#define RESERVED_BYTES   64

/* Size of each partition (should be 12 ideally). */
#define PARTITION_SIZE   16

#define MAX_PARTITIONS   (8-1) /* Arbitrary. */
#define VALID_MAP_MAGIC  0xDEADBEEF

#define FLASH_MAP_OK        0
#define FLASH_MAP_NOT_INIT -1
#define FLASH_MAP_FULL     -2
#define FLASH_MAP_OOB      -3

#define PARTITION_NO 2 /* TODO */

int create_partition_table(struct partition_header_info *info)
{
	int ret;
	const struct flash_area *fap;
	const uint8_t magic[]  = {0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x00, 0x00}; /* Need to find a better way to do this tbh.. */
	const uint8_t dev_id[] = {info->flash_dev_id, 0x00, 0x00, 0x00};

	if (info == NULL)
		return -EIO;

	ret = flash_area_open(PARTITION_NO, &fap);
	if (ret != 0)
		return ret;

	/* Erase before writing. */
	ret = flash_area_erase(fap, 0, 4096);
	if (ret != 0)
		goto exit;

	/* Write Magic (and clear partition count). */
	ret = flash_area_write(fap, 0, (const void *) magic, sizeof(magic));
	if (ret != 0)
		goto exit;

	/* Write Dev Id. */
	ret = flash_area_write(fap, 8, (const void *) dev_id, sizeof(dev_id));
	if (ret != 0)
		goto exit;

	/* Write Dev Name. */
	ret = flash_area_write(fap, 12, (const void *) info->flash_dev_name, sizeof(info->flash_dev_name));
	if (ret != 0)
		goto exit;

exit:
	flash_area_close(fap);
	return ret;
}

int add_partition(struct flash_partition_info *partition)
{
	uint32_t offset;
	struct flash_map_info map;
	const struct flash_area *fap;
	int ret;

	if (partition == NULL)
		return -EIO;

	ret = flash_area_open(PARTITION_NO, &fap);
	if (ret != 0)
		return ret;

	/* Read header */
	ret = flash_area_read(fap, 0, (void *) &map, sizeof(struct flash_map_info));
	if (ret != 0)
		goto exit;

	if (map.map_magic != 0xDEADBEEF)
	{
		ret = FLASH_MAP_NOT_INIT;
		goto exit;
	}

	if (map.partition_cnt >= MAX_PARTITIONS)
	{
		ret = FLASH_MAP_FULL;
		goto exit;
	}

	/* Add new partition. */
	offset = (map.partition_cnt++ * PARTITION_SIZE) + RESERVED_BYTES;
	ret = flash_area_write(fap, offset, (const void *) partition, sizeof(struct flash_partition_info));
	if (ret != 0)
		goto exit;

	ret = flash_area_erase(fap, 0, sizeof(struct flash_map_info));
	ret = flash_area_write(fap, 0, (const void *) &map, sizeof(struct flash_map_info));

exit:
	flash_area_close(fap);
	return ret;
}

int get_partition(struct flash_partition_info *partition, int part_no)
{
	uint32_t offset;
	struct flash_map_info map;
	const struct flash_area *fap;
	int ret;

	if (partition == NULL)
		return -EIO;

	ret = flash_area_open(PARTITION_NO, &fap);
	if (ret != 0)
		return ret;

	/* Read header */
	ret = flash_area_read(fap, 0, (void *) &map, sizeof(struct flash_map_info));
	if (ret != 0)
	{
		goto exit;
		return ret;
	}

	if (map.map_magic != 0xDEADBEEF)
	{
		ret = FLASH_MAP_NOT_INIT;
		goto exit;
	}

	if (part_no >= map.partition_cnt)
	{
		ret = FLASH_MAP_OOB;
		goto exit;
	}

	/* Read partition info. */
	offset = (part_no * PARTITION_SIZE) + RESERVED_BYTES;
	ret = flash_area_read(fap, offset, (void *) partition, sizeof(struct flash_partition_info));

exit:
	flash_area_close(fap);
	return ret;
}

/* Just deletes the magic, partition count. Everything else is still
 * the same. */
int delete_partition_table(void)
{
	/* TODO */
//	memset(&buffer[0], 0xFF, sizeof(struct flash_map_info));
	return 0; /* Will probably change when we introduce flash. */
}
