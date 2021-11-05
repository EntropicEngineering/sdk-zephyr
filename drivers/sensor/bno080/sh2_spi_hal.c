/*
 * Copyright 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT hillcrest_bno08x

#include <zephyr.h>
#include <kernel.h>
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
static struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) |
				 SPI_TRANSFER_MSB |
				 SPI_OP_MODE_MASTER |
				 SPI_MODE_CPOL |
				 SPI_MODE_CPHA,
	.frequency = 2000000,
};
static const struct device *bno08x_dev;

static struct k_sem sh2_lock;
static struct gpio_callback sh2_irq_data;

static struct k_thread sh2_irq_thread;
static K_THREAD_STACK_DEFINE(sh2_irq_thread_stack, 4096);

struct sh2_event {
    uint32_t t_uS;
};

K_MSGQ_DEFINE(sh2_queue, sizeof(struct sh2_event), 10, 4);

static uint8_t txBuf[SH2_HAL_MAX_TRANSFER];
static uint16_t txLen;

static uint8_t rxBuf[SH2_HAL_MAX_TRANSFER];
static uint16_t rxLen;

static void sh2_irq_handler(const struct device *dev, struct gpio_callback *cb,
	    uint32_t pins)
{
	const struct bno08x_config *config = bno08x_dev->config;

	struct sh2_event event = {
		.t_uS = k_cycle_get_32(),
	};

	k_msgq_put(&sh2_queue, &event, K_NO_WAIT);
}

static const uint8_t txZeros[SH2_HAL_MAX_TRANSFER];
static const uint8_t* spiTxData;
static uint8_t* spiRxData;


static uint16_t start_spi_transfer(struct sh2_event *event)
{
	const struct bno08x_config *config = bno08x_dev->config;
	uint8_t tmp[2] = {0};
	struct spi_buf spibuf[2] = {
		{.buf = tmp, .len = 2},
		{.buf = rxBuf, .len = 2},
	};
	struct spi_buf_set spiset[2] = {
		{.buffers = &spibuf[0], .count = 1},
		{.buffers = &spibuf[1], .count = 1},
	};


	int ret;

	/* Desssert wake if TX is to be done: TODO */
//	if (txLen != 0) {
//		gpio_pin_set(config->wake.port, config->wake.pin, 1);
//		spibuf[0].buf = txBuf;
//	}

	ret = spi_transceive(spi, &spi_cfg, &spiset[0], &spiset[1]);
	if (ret != 0)
	{
		LOG_ERR("Failed to start spi transfer (ret: %d)", ret);
		// end_spi_transfer
		/* return 0x7FFF; */
	}

	uint8_t *tx = (uint8_t *) spibuf[0].buf;
	uint8_t *rx = (uint8_t *) spibuf[1].buf;

    uint16_t tx_len = (tx[0] + (tx[1] << 8) & ~0x8000);
    uint16_t rx_len = (rx[0] + (rx[1] << 8) & ~0x8000);
    if (rx_len == 0x7FFF) {
        // 0x7FFF is an invalid length
        rxLen = 0;
    }

    uint16_t len = (tx_len > rx_len) ? tx_len : rx_len;
    if (len > SH2_HAL_MAX_TRANSFER) {
        len = SH2_HAL_MAX_TRANSFER;
    }

    if (len == 0)
    {

    }

    return 0;
}

static void do_spi_transfer(uint16_t len)
{

}

static void sh2_irq_thread_func(void *dummy1, void *dummy2, void *dummy3)
{
	while (true)
	{
		struct sh2_event event;
		uint16_t len;

		if (k_msgq_get(&sh2_queue, (void *) &event, K_FOREVER) != 0)
			continue;

		len = start_spi_transfer(&event);
		if (len > SH2_HAL_MAX_TRANSFER)
			len = SH2_HAL_MAX_TRANSFER;

		do_spi_transfer(len);

	}
}

static int setup_sh2_pins(const struct gpio_dt_spec *bootn,
		const struct gpio_dt_spec *nrst, const struct gpio_dt_spec *wake)
{
	int ret;

#if 0
	ret = gpio_pin_configure_dt(bootn, /* GPIO_OUTPUT_HIGH | */ GPIO_ACTIVE_LOW);
	if (ret != 0) {
		LOG_ERR("gpio_pin_configure_dt failed (ret: %d)", ret);
		return ret;
	}
#endif

	ret = gpio_pin_configure_dt(nrst, /* GPIO_OUTPUT_LOW | */ GPIO_ACTIVE_LOW);
	if (ret != 0) {
		LOG_ERR("gpio_pin_configure_dt failed (ret: %d)", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(wake, /* GPIO_OUTPUT_HIGH | */ GPIO_ACTIVE_HIGH);
	if (ret != 0) {
		LOG_ERR("gpio_pin_configure_dt failed (ret: %d)", ret);
		return ret;
	}

	return 0;
}

static int setup_sh2_intn(const struct gpio_dt_spec *pin)
{
	int ret;

	ret = gpio_pin_configure_dt(pin, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("gpio_pin_configure_dt failed (ret: %d)", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(pin, GPIO_INT_EDGE_BOTH /* GPIO_INT_LEVEL_LOW */ );
	if (ret != 0) {
		LOG_ERR("gpio_pin_interrupt_configure_dt failed (ret: %d)", ret);
		return ret;
	}

	gpio_init_callback(&sh2_irq_data, sh2_irq_handler, BIT(pin->pin));
	gpio_add_callback(pin->port, &sh2_irq_data);

	LOG_INF("sh2 interrupt setup successful");

	return 0;
}

int sh2_hal_reset(bool dfuMode, sh2_rxCallback_t *onRx, void *cookie)
{
	const struct bno08x_config *config = bno08x_dev->config;
	int ret;

	gpio_pin_set(config->wake.port, config->wake.pin, 0);

	/* Assert Reset. */
	gpio_pin_set(config->nrst.port, config->nrst.pin, 0);

	/* waken = 1, bootn = 0 (dfu not supported). */
//	gpio_pin_set(config->bootn.port, config->bootn.pin, 0);
//	gpio_pin_set(config->wake.port, config->wake.pin, 1);

	/* Enable sh2 interrupt. */
//	ret = setup_sh2_intn(&config->intn);
	ret = 0;
	if (ret != 0) {
		LOG_ERR("setup_sh2_intn failed (ret: %d)", ret);
		LOG_ERR("sh2_hal_reset failed");
		return ret;
	}

#if 1
	static struct spi_cs_control cs;

	cs.gpio_dev = device_get_binding(DT_INST_SPI_DEV_CS_GPIOS_LABEL(0));
	cs.gpio_pin = DT_INST_SPI_DEV_CS_GPIOS_PIN(0);
	cs.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0);
	cs.delay = 0U;

	ret = gpio_pin_configure(cs.gpio_dev, cs.gpio_pin, cs.gpio_dt_flags);

	spi_cfg.cs = (struct spi_cs_control *) &cs;
#endif

	/* This reset delay of 10ms is arbitrary (taken from ST implementation)
	 * and there should be a better way to wait for the reset to take effect. */
	k_usleep(10000);

	/* Deassert Reset. */
	gpio_pin_set(config->nrst.port, config->nrst.pin, 1);

	LOG_INF("sh2_hal_reset done");
	return 0;
}

int sh2_hal_tx(uint8_t *pData, uint32_t len)
{
	const struct bno08x_config *config = bno08x_dev->config;

	/* TODO: Take lock here. */

	memcpy(txBuf, pData, len);
	txLen = len;

	/* Assert Wake. */
	gpio_pin_set(config->wake.port, config->wake.pin, 0);

	return 0;
}

int sh2_hal_rx(uint8_t *pData, uint32_t len)
{
	return -ENOTSUP;
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
	int ret;

	spi = device_get_binding(config->bus_name);
	if (spi == NULL) {
		LOG_DBG("spi device not found: %s", config->bus_name);
		return -EINVAL;
	}

//	volatile unsigned x = 0;
//	while (x == 0);

	/* Set up the block/unblock lock. */
	k_sem_init(&sh2_lock, 0, 1);

	// ret = setup_sh2_pins(&config->bootn, &config->nrst, &config->wake);

    k_thread_create(&sh2_irq_thread, sh2_irq_thread_stack,
            K_THREAD_STACK_SIZEOF(sh2_irq_thread_stack),
			sh2_irq_thread_func, NULL, NULL, NULL,
            7 /* TODO */, 0,
			K_NO_WAIT);

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
