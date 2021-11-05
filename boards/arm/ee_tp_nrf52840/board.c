/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <drivers/gpio.h>
#include <device.h>
#include <devicetree.h>
#include <hal/nrf_power.h>

static struct gpio_dt_spec gpio_dt_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(power_hold), gpios);
#define VOUT        UICR_REGOUT0_VOUT_3V3

static int board_ee_tp_nrf52840_init(const struct device __unused *dev) {

    (void) gpio_pin_configure_dt(&gpio_dt_spec, GPIO_OUTPUT_ACTIVE);

	if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) != (VOUT << UICR_REGOUT0_VOUT_Pos)) {

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
		    ;

        NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~((uint32_t) UICR_REGOUT0_VOUT_Msk)) | (VOUT << UICR_REGOUT0_VOUT_Pos);

		while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
		    ;

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
            ;

        // System reset is needed to update UICR registers.
        NVIC_SystemReset();
    }
	return 0;
}

SYS_INIT(board_ee_tp_nrf52840_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
