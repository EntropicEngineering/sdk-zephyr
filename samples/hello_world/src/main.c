/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

void main(void)
{
	const struct device *gpio0, *gpio1;
	const gpio_pin_t gpio0_pins[] = {
		22, /* MISO */
		25, /* MOSI */
		24, /* CSN */
		9,  /* NRST */
		10, /* INTN */
	};
	const gpio_pin_t gpio1_pins[] = {
		0,  /* SCK */
		2,  /* WAKE */
		1,  /* BOOTN */
	};
	int ret;

	printk("Hello World! %s\n", CONFIG_BOARD);

	gpio0 = device_get_binding("GPIO_0");
	gpio1 = device_get_binding("GPIO_1");
	if (!gpio0 || !gpio1)
		while (1);

	for (int i = 0; i < sizeof(gpio0_pins) / sizeof(int); i ++) {
		ret = gpio_pin_configure(gpio0, gpio0_pins[i], GPIO_OUTPUT);
		if (ret)
			while (1);
	}

	for (int i = 0; i < sizeof(gpio1_pins) / sizeof(int); i ++) {
		ret = gpio_pin_configure(gpio1, gpio1_pins[i], GPIO_OUTPUT);
		if (ret)
			while (1);
	}

	bool one_or_zero = true;
	volatile int iter = 0;

	while (1)
	{
		for (int i = 0; i < sizeof(gpio0_pins) / sizeof(int); i ++) {
			ret = gpio_pin_set(gpio0, gpio0_pins[i], one_or_zero);
			if (ret)
				while (1);
		}

		for (int i = 0; i < sizeof(gpio1_pins) / sizeof(int); i ++) {
			ret = gpio_pin_set(gpio1, gpio1_pins[i], one_or_zero);
			if (ret)
				while (1);
		}

		iter ++;

		one_or_zero = !one_or_zero;
		k_msleep(1000); /* 1sec */
	}

}
