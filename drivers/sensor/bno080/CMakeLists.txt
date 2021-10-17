# Copyright (c) 2021 Entropic Engg
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# Add HAL Implementation
zephyr_library_sources(sh2_spi_hal.c)

# Add Library Files
zephyr_library_sources(bno080-driver/sh2_SensorValue.c)
zephyr_library_sources(bno080-driver/sh2_util.c)
zephyr_library_sources(bno080-driver/sh2.c)
zephyr_library_sources(bno080-driver/shtp.c)
zephyr_library_sources(bno080-driver/worldTare.c)

zephyr_include_directories(.)
zephyr_include_directories(bno080-driver)
