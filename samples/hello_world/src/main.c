/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <drivers/flash.h>
#include <storage/flash_map_dynamic.h>
#include <fs/nvs.h>

struct partition_header_info info = {
	.flash_dev_id = 0,
	.flash_dev_name = "NRF_FLASH_DRV_NAME",
};

struct flash_partition_info parts[] = {
	{
		.fa_id   = 0,
		.fa_off  = 0xD000,
		.fa_size = 0x1000,
	},
	{
		.fa_id = 1,
		.fa_off  = 0xE000,
		.fa_size = 0x1000,
	},
	{
		.fa_id = 2,
		.fa_off  = 0xF000,
		.fa_size = 0x1000,
	},
	{
		.fa_id = 3,
		.fa_off  = 0x10000,
		.fa_size = 0x1000,
	},
	{
		.fa_id = 4,
		.fa_off  = 0x20000,
		.fa_size = 0x1000,
	},
};

void main(void)
{
	int ret, i;

	printk("Hello World! %s\n", CONFIG_BOARD);

	ret = create_partition_table(&info);
	if (ret != 0)
	{
		printk("create_partition_table failed (ret=%d) \r\n", ret);
		while (1);
	}

	for (i = 0; i < sizeof(parts) / sizeof(struct flash_partition_info); i ++)
	{
		ret = add_partition(&parts[i]);
		if (ret != 0)
		{
			printk("add_partition failed (ret=%d) \r\n", ret);
			while (1);
		}
	}

	struct flash_partition_info tmp;
	for (i = 0; i < sizeof(parts) / sizeof(struct flash_partition_info); i ++)
	{
		ret = get_partition(&tmp, i);
		if (ret != 0)
		{
			printk("get_partition failed (ret=%d) \r\n", ret);
			while (1);
		}

		printk("fa_id = %x, fa_off = %x, fa_size = %x \r\n", tmp.fa_id, tmp.fa_off, tmp.fa_size);
	}

}
