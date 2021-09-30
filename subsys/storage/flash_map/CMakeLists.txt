# SPDX-License-Identifier: Apache-2.0

zephyr_sources(flash_map.c)
zephyr_sources_ifndef(CONFIG_FLASH_MAP_CUSTOM flash_map_default.c)
zephyr_sources_ifdef(CONFIG_FLASH_MAP_SHELL flash_map_shell.c)

# Enable dynamic flash map support alongside the "default" one.
# Whenever needed, mcuboot can go in and change the map.
zephyr_sources_ifdef(CONFIG_FLASH_MAP_DYNAMIC flash_map_dynamic.c)
