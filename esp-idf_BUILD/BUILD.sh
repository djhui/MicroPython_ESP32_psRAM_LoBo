#!/bin/bash

# #################################################################
# This script makes it easy to build MicroPython firmware for ESP32
# #################################################################

# Usage:
# ./BUILD               - run the build, create MicroPython firmware
# ./BUILD -j4           - run the build on multicore system, much faster build
#                         replace 4 with the number of cores on your system
# ./BUILD menuconfig    - flash MicroPython firmware to ESP32
# ./BUILD clean         - clean the build
# ./BUILD flash         - flash MicroPython firmware to ESP32
# ./BUILD erase         - erase the whole ESP32 Flash
# ./BUILD monitor       - run esp-idf terminal program

export HOST_PLATFORM=linux

# ########################################
# Add Xtensa toolchain path to system path
# ########################################
export PATH=/home/LoBo2_Razno/ESP32/MicroPython_ESP32_psRAM_LoBo/xtensa-esp32-elf_psram/bin:$PATH

# ###################
# Export esp-idf path
# ###################
export IDF_PATH=/home/LoBo2_Razno/ESP32/MicroPython_ESP32_psRAM_LoBo/esp-idf_psram

make "$@"
