# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
<<<<<<< HEAD
=======

# Hardware definition
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

>>>>>>> d4571a35cbe754a8129320a14981554516df7e8c
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bno080)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
