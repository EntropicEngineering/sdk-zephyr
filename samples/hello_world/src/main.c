/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>
#include <device.h>
#include <storage/flash_map.h>
#include <drivers/flash.h>
#include <soc.h>
#include <init.h>

#include <storage/flash_map_dynamic.h>

void main(void)
{
	int i, rc;

	printk("Hello World! %s\n", CONFIG_BOARD);

	/* First time write (assuming blank settings). */
	{
		struct flash_partition_info parts[] = {
			{
				.fa_id = IMAGE_0_PARTITION_ID,
				.fa_off  = 0x00030000,
				.fa_size = 0x00020000,
			},
			{
				.fa_id = IMAGE_1_PARTITION_ID,
				.fa_off  = 0x00050000,
				.fa_size = 0x00020000,
			},
		};

		for (i = 0; i < sizeof(parts) / sizeof(struct flash_partition_info); i ++)
		{
			rc = add_partition_at_end(&parts[i]);
			if (rc != 0)
				while (1);
		}
	}

	/* Write by Index */
	{
		struct flash_partition_info new1[] = {
			{
				.fa_id = IMAGE_0_PARTITION_ID,
				.fa_off  = 0x00030000,
				.fa_size = 0x00040000,
			},
			{
				.fa_id = IMAGE_1_PARTITION_ID,
				.fa_off  = 0x00070000,
				.fa_size = 0x000b0000,
			},
		};

		for (i = 0; i < sizeof(new1) / sizeof(struct flash_partition_info); i ++)
		{
			rc = add_partition_at_index(i, &new1[i]);
			if (rc != 0)
				while (1);
		}
	}

	/* Write by Id */
	{
		struct flash_partition_info new2[] = {
			{
				.fa_id = IMAGE_0_PARTITION_ID,
				.fa_off  = 0x00030000,
				.fa_size = 0x00030000,
			},
			{
				.fa_id = IMAGE_1_PARTITION_ID,
				.fa_off  = 0x00060000,
				.fa_size = 0x00090000,
			},
		};

		for (i = 0; i < sizeof(new2) / sizeof(struct flash_partition_info); i ++)
		{
			rc = add_partition_by_id(new2[i].fa_id, &new2[i]);
			if (rc != 0)
				while (1);
		}
	}

}
