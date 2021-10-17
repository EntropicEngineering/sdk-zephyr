/*
 * Copyright 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sh2_hal.h"

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
	return 0;
}

int sh2_hal_unblock(void)
{
	return 0;
}
