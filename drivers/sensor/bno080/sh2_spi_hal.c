/*
 * Copyright 2021-2022 Entropic Engg
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

#define SH2_IRQ_TASK_STACK (4096)
#define SH2_IRQ_TASK_PRIO  (0)
#define SH2_QUEUE_DEPTH    (10)
#define SH2_QUEUE_ALIGN    (4)

struct sh2_event {
    /* 32-bit timestamp, in microseconds, associated with the INTN signal assertion. */
    uint32_t timestamp;
};

struct bno08x_config {
	char *bus_name;
	struct gpio_dt_spec nrst;
	struct gpio_dt_spec intn;
	struct gpio_dt_spec wake;
	struct gpio_dt_spec bootn;
};

struct bno08x_data {

	/* SPI device (and its config) to which the BNO chip is connected. */
	const struct device *spi;
	struct spi_config spi_cfg;
	struct spi_cs_control chip_select;

	/* SH2 locks for:
	 * .. SH2 HAL Block / Unblock routines.
	 * .. SH2 TX Locking (only 1 tx at a time). */
	struct k_sem sh2_lock;
	struct k_sem sh2_tx;

	/* SH2 INTN callback. */
	struct gpio_callback sh2_irq_data;

	/* Request processing thead (and its stack). */
	struct k_thread sh2_irq_thread;
	/* struct z_thread_stack_element *sh2_irq_thread_stack; */

	/* Queue to transfer data between IRQ and Thread. */
	struct k_msgq sh2_queue;
	struct sh2_event sh2_queue_buf[SH2_QUEUE_DEPTH];

	/* SH2 specific:
	 * .. Cookie
	 * .. Callback */
	sh2_rxCallback_t *rx_callback;
	void *cookie;

	/* SH2 buffers to do TX / RX. */
	uint8_t tx_buf[SH2_HAL_MAX_TRANSFER];
	uint8_t rx_buf[SH2_HAL_MAX_TRANSFER];

	/* Variable set by the SH2 HAL TX API, and used by the SH2 Processing thread.
	 * This tells the thread if the interrupt was due to:
	 * .. MCU wanting to send data to the BNO chip.
	 * .. An unsolicited interrupt from BNO chip.
	 * As there is only a single TX at a time, a single variable should work fine. */
	uint16_t sh2_tx_len;
};

/* Global pointer of the bno080 device. Have to store this as a global variable as the SH2
 * APIs have no way of passing this to the HAL funcs. */
static const struct device *bno08x_dev;

static K_THREAD_STACK_DEFINE(sh2_irq_thread_stack, 4096); /* TODO */

/* Array to zeros to trigger a transfer.
 * TODO: better way to do this? */
static uint8_t zeros[SH2_HAL_MAX_TRANSFER];

static void sh2_irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int ret;
	struct bno08x_data *data = bno08x_dev->data; /* TODO */

	struct sh2_event event = {
		.timestamp = k_cycle_get_32(),
	};

	ret = k_msgq_put(&data->sh2_queue, &event, K_NO_WAIT);
	if (ret < 0) {
		LOG_ERR("%s: sh2_queue put error (ret: %d)", __func__, ret);
	}
}

/* SH2 header is 4 bytes - 2 bytes for data, the remaining 2 have other info. Though this
 * function says that it reads the SH2 header, it only gets the first 2 bytes (to get the length)
 * and not all 4. */
static int sh2_spi_header_tx(const struct device *dev, struct sh2_event *event,
		uint16_t *transaction_len)
{
	const struct bno08x_config *config = dev->config;
	struct bno08x_data *data = dev->data;
	int ret;

	/* Set up 2 "spi_buf",
	 * .. TX buffer (size = 2), sends zeros.
	 * .. RX buffer, receives 2 bytes. */
	struct spi_buf spibuf[2] = {
		{
			.buf = zeros,
			.len = 2
		},
		{
			.buf = data->rx_buf,
			.len = 2
		},
	};

	/* Put the above created bufs to the spi set. */
	struct spi_buf_set spiset[2] = {
		{
			.buffers = &spibuf[0],
			.count = 1
		},
		{
			.buffers = &spibuf[1],
			.count = 1
		},
	};

	/* If we are doing a TX (instead of receiving unsolicited interrupts from BNO), de-assert the
	 * wake pin and use the correct buffer for TX (don't send 0s). */
	if (data->sh2_tx_len) {
		gpio_pin_set(config->wake.port, config->wake.pin, 1);
		spibuf[0].buf = data->tx_buf;
	}

	ret = spi_transceive(data->spi, &data->spi_cfg, &spiset[0], &spiset[1]);
	if (ret) {
		LOG_ERR("SPI Failed: Unable to do SH2 header transfer (ret: %d)", ret);
		return ret;
	}

	uint8_t *tx = (uint8_t *) spibuf[0].buf;
	uint8_t *rx = (uint8_t *) spibuf[1].buf;

    uint16_t tx_len = ((tx[0] + (tx[1] << 8)) & 0x7FFF);
    uint16_t rx_len = ((rx[0] + (rx[1] << 8)) & 0x7FFF);

    *transaction_len = (tx_len > rx_len) ? tx_len : rx_len;
    if (*transaction_len > SH2_HAL_MAX_TRANSFER) {
    	*transaction_len = SH2_HAL_MAX_TRANSFER;
    }

    return 0;
}

static void sh2_spi_transfer(const struct device *dev, struct sh2_event *event)
{
	struct bno08x_data *data = dev->data;
	int ret;
	uint16_t transaction_len;

	ret = sh2_spi_header_tx(dev, event, &transaction_len);
	if (ret) {
		LOG_ERR("sh2_spi_header_tx failed (ret: %d)", ret);
		return;
	}

	if (transaction_len <= 2) {
		LOG_ERR("Header transfer successful, but transaction_len <= 2. Do nothing");
		return;
	}

	struct spi_buf spibuf[2] = {
		{
			.buf = data->sh2_tx_len ? &data->tx_buf[2] : &zeros[2],
			.len = transaction_len - 2
		},
		{
			.buf = &data->rx_buf[2],
			.len = transaction_len - 2
		},
	};

	struct spi_buf_set spiset[2] = {
		{
			.buffers = &spibuf[0],
			.count = 1
		},
		{
			.buffers = &spibuf[1],
			.count = 1
		},
	};

	ret = spi_transceive(data->spi, &data->spi_cfg, &spiset[0], &spiset[1]);
	if (ret) {
		LOG_ERR("SPI Failed: Unable to do SH2 data transfer (ret: %d)", ret);
		return;
	}

	/* Invoke RX callback (send data to middleware stack), and release the TX lock (if applicable). */
	data->rx_callback(data->cookie, data->rx_buf, transaction_len, event->timestamp);
	if (data->sh2_tx_len != 0) {
		data->sh2_tx_len = 0;
		k_sem_give(&data->sh2_tx);
	}
}

/* Func: sh2_process_request
 * Desc: When an interrupt is received from the BNO chip, an irq is triggered (sh2_irq_handler) telling us that the
 *       BNO chip wants to talk. The irq handler does not process the message itself and forwards the request to this
 *       thread to ensure that no starvation happens. All communication with the BNO chip happens from the context of
 *       this thread. */
static void sh2_process_request(void *dummy1, void *dummy2, void *dummy3)
{
	const struct device *dev = (const struct device *) dummy1;
	struct bno08x_data *data;

	if (dev == NULL) {
		LOG_ERR("Invalid dev pointer to %s func, Trapping.", __func__);
		while (1);
	}

	data = dev->data;

	while (true)
	{
		struct sh2_event event;
		int ret;

		ret = k_msgq_get(&data->sh2_queue, (void *) &event, K_FOREVER);
		if (ret != 0) {
			LOG_ERR("%s: sh2_queue get error (ret: %d)", __func__, ret);
			continue;
		}

		sh2_spi_transfer(dev, &event);
	}
}

/* Func: setup_sh2_intn
 * Desc: This function sets up the BNO IRQ pin. Registers callback function so that interrupts
 *       from the chip are not missed. */
static int setup_sh2_intn(const struct device *dev, const struct gpio_dt_spec *pin)
{
	int ret;
	struct bno08x_data *data = dev->data;

	ret = gpio_pin_configure_dt(pin, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("intn pin configure failed (ret: %d)", ret);
		return ret;
	}

    ret = gpio_pin_interrupt_configure_dt(pin, GPIO_INT_EDGE_FALLING);
	if (ret != 0) {
		LOG_ERR("intn pin interrupt configure failed (ret: %d)", ret);
		return ret;
	}

	/* Add interrupt callback. */
	gpio_init_callback(&data->sh2_irq_data, sh2_irq_handler, BIT(pin->pin));
	ret = gpio_add_callback(pin->port, &data->sh2_irq_data);
	if (ret != 0) {
		LOG_ERR("sh2 intn add_callback failed (ret: %d)", ret);
		return ret;
	}

	LOG_INF("sh2 interrupt setup successful");
	return 0;
}

/* Func: setup_sh2_pins
 * Desc: This function sets up the BNO pins (bootn, nrst, wake) properly by setting them as
 *       output and using appropriate gpio_flags. */
static int setup_sh2_pins(const struct device *dev)
{
	int ret;
	const struct bno08x_config *config = dev->config;
	struct bno08x_data *data = dev->data;

	ret = gpio_pin_configure_dt(&config->nrst, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (ret != 0) {
		LOG_ERR("nrst pin configure failed (ret: %d)", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&config->wake, GPIO_OUTPUT | GPIO_ACTIVE_HIGH);
	if (ret != 0) {
		LOG_ERR("wake pin configure failed (ret: %d)", ret);
		return ret;
	}

	/* Enable sh2 interrupt. */
	ret = setup_sh2_intn(dev, &config->intn);
	if (ret != 0) {
		LOG_ERR("intn pin configure failed (ret: %d)", ret);
		return ret;
	}

	/* Set up CS.
	 * TODO: Fix up hacks (may be?) */
	data->chip_select.gpio_dev = device_get_binding(DT_INST_SPI_DEV_CS_GPIOS_LABEL(0));
	data->chip_select.gpio_pin = DT_INST_SPI_DEV_CS_GPIOS_PIN(0);
	data->chip_select.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0);
	data->chip_select.delay = 0U;

	ret = gpio_pin_configure(data->chip_select.gpio_dev, data->chip_select.gpio_pin, data->chip_select.gpio_dt_flags);
	if (ret != 0) {
		LOG_ERR("failed to configure cs pin (ret: %d)", ret);
		return ret;
	}

	data->spi_cfg.cs = (struct spi_cs_control *) &data->chip_select;
	LOG_INF("sh2 pin setup successful");
	return 0;
}

/* ------------------------- SH2 HAL FUNCTION IMPLEMENTATION BEGINS ------------------------- */

int sh2_hal_reset(bool dfuMode, sh2_rxCallback_t *onRx, void *cookie)
{
	const struct bno08x_config *config = bno08x_dev->config;
	struct bno08x_data *data = bno08x_dev->data;

	/* Assert Reset / Wake. */
	gpio_pin_set(config->wake.port, config->wake.pin, 1);
	gpio_pin_set(config->nrst.port, config->nrst.pin, 1);

	/* Save callback / cookie. */
	data->rx_callback = onRx;
	data->cookie = cookie;

	/* This reset delay of 10ms is arbitrary (taken from ST implementation)
	 * and there should be a better way to wait for the reset to take effect. */
	 k_usleep(10000);

	/* Deassert Reset. */
	gpio_pin_set(config->nrst.port, config->nrst.pin, 0);
	LOG_INF("sh2_hal_reset done");
	return 0;
}

int sh2_hal_tx(uint8_t *pData, uint32_t len)
{
	const struct bno08x_config *config = bno08x_dev->config;
	struct bno08x_data *data = bno08x_dev->data;
	int ret;

	ret = k_sem_take(&data->sh2_tx, K_FOREVER);
	if (ret < 0) {
		LOG_ERR("Failed to take sh2_tx lock (ret: %d)", ret);
		return ret;
	}

	if (len > sizeof(data->tx_buf)) {
		LOG_ERR("Cannot process the TX Transaction.");
		LOG_ERR("Requested Data Size: %d, Size of the buffer: %d", len, sizeof(data->tx_buf));
		return -1;
	}

	/* Copy data / Len, and lets go. */
	memcpy(data->tx_buf, pData, len);
	data->sh2_tx_len = len;

	gpio_pin_set(config->wake.port, config->wake.pin, 0);
	return 0;
}

int sh2_hal_rx(uint8_t *pData, uint32_t len)
{
	return -ENOTSUP;
}

int sh2_hal_block(void)
{
	struct bno08x_data *data = bno08x_dev->data;

	return k_sem_take(&data->sh2_lock, K_FOREVER);
}

int sh2_hal_unblock(void)
{
	struct bno08x_data *data = bno08x_dev->data;

	k_sem_give(&data->sh2_lock);
	return 0;
}

/* ------------------------- SH2 HAL FUNCTION IMPLEMENTATION ENDS ------------------------- */

static int sh2_spi_hal_init(const struct device *dev)
{
	const struct bno08x_config *config = dev->config;
	struct bno08x_data *data = dev->data;
	int ret;

	data->spi = device_get_binding(config->bus_name);
	if (data->spi == NULL) {
		LOG_ERR("spi device not found: %s", config->bus_name);
		return -EINVAL;
	}

	/* Set up the block/unblock lock. */
	ret = k_sem_init(&data->sh2_lock, 0, 1);
	if (ret) {
		LOG_ERR("sh2_lock init failed (ret: %d)", ret);
		return ret;
	}

	/* Set up the TX lock (only 1 TX at a time). */
	ret = k_sem_init(&data->sh2_tx, 1, 1);
	if (ret) {
		LOG_ERR("sh2_tx lock init failed (ret: %d)", ret);
		return ret;
	}

	/* Set up Queue b/w IRQ and Thread. */
	k_msgq_init(&data->sh2_queue, (char *)data->sh2_queue_buf,
		    sizeof(struct sh2_event), SH2_QUEUE_DEPTH);

    k_thread_create(&data->sh2_irq_thread, sh2_irq_thread_stack,
            K_THREAD_STACK_SIZEOF(sh2_irq_thread_stack),
			sh2_process_request, (void *) dev, NULL, NULL,
			SH2_IRQ_TASK_PRIO, 0, K_NO_WAIT);

	ret = setup_sh2_pins(dev);
	if (ret) {
		LOG_ERR("Failed to set up sh2 pins (ret: %d)", ret);
		return ret;
	}

	/* Function calls from the library do not contain information
	 * about the device which means we need to store this info
	 * globally. Logically this limits us to a single sensorhub
	 * instance for now :-( */
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
	static struct bno08x_data bno08x_data_##index = {                      	  \
		.spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |             \
			 	 	 	 	 SPI_OP_MODE_MASTER | SPI_MODE_CPOL |             \
							 SPI_MODE_CPHA,                                   \
        .spi_cfg.frequency = 1000000,                                         \
	};                                                                        \
									                                          \
	DEVICE_DT_INST_DEFINE(index, &sh2_spi_hal_init, NULL,                     \
				&bno08x_data_##index, &bno08x_config_##index,                 \
				POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(BNO08X_INIT)
