/*
 * Copyright 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT hillcrest_bno08x

#include <zephyr.h>
#include <drivers/spi.h>
#include <logging/log.h>
#include <drivers/gpio.h>

#include "sh2_hal.h"

LOG_MODULE_REGISTER(BNO08X, CONFIG_SENSOR_LOG_LEVEL);

struct bno08x_config {
	char *bus_name;
	struct gpio_dt_spec nrst;
	struct gpio_dt_spec intn;
	struct gpio_dt_spec wake;
	struct gpio_dt_spec bootn;
};

static const struct device *spi;
static struct spi_config spi_cfg;
static const struct device *bno08x_dev;

static struct k_sem sh2_lock;

int sh2_hal_reset(bool dfuMode, sh2_rxCallback_t *onRx, void *cookie)
{
	return 0;
}

int sh2_hal_tx(uint8_t *pData, uint32_t len)
{
	return 0;
}

int sh2_hal_rx(uint8_t *pData, uint32_t len)
{
	return 0;
}

int sh2_hal_block(void)
{
	return k_sem_take(&sh2_lock, K_FOREVER);
}

int sh2_hal_unblock(void)
{
	k_sem_give(&sh2_lock);
	return 0;
}

static int sh2_spi_hal_init(const struct device *dev)
{
	const struct bno08x_config *config = dev->config;

	spi = device_get_binding(config->bus_name);
	if (spi == NULL) {
		LOG_DBG("spi device not found: %s", config->bus_name);
		return -EINVAL;
	}

	/* Set up the block/unblock lock. */
	k_sem_init(&sh2_lock, 0, 1);

	spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
	/* spi_cfg.frequency = config->spi_max_frequency; */

	gpio_pin_set(config->nrst.port, config->nrst.pin, 0);
	gpio_pin_set(config->bootn.port, config->bootn.pin, 1);
	gpio_pin_set(config->wake.port, config->wake.pin, 1);

	/* Function calls from the library do not contain information
	 * about the device which means we need to store this info
	 * globally. Logically this limits us to a single sensorhub
	 * instance for now.. Shouldn't be a problem for now (or ever). */
	bno08x_dev = dev;

	return 0;
}

#define BNO08X_INIT(index)                                                    \
	static const struct bno08x_config bno08x_config_##index = {               \
		.bus_name = DT_INST_BUS_LABEL(index),                                 \
		.nrst     = GPIO_DT_SPEC_INST_GET(index, nrst_gpios),                 \
		.intn     = GPIO_DT_SPEC_INST_GET(index, intn_gpios),                 \
		.wake     = GPIO_DT_SPEC_INST_GET(index, wake_gpios),                 \
		.bootn    = GPIO_DT_SPEC_INST_GET(index, bootn_gpios),                \
	};                                                                        \
									                                          \
	DEVICE_DT_INST_DEFINE(index, &sh2_spi_hal_init, NULL,                     \
			    NULL, &bno08x_config_##index, POST_KERNEL,                    \
			    CONFIG_SENSOR_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(BNO08X_INIT)
