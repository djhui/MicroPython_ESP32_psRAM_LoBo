#!/bin/bash

# #################################################################
# This script makes it easy to build MicroPython firmware for ESP32
# #################################################################

# Usage:
# ./BUILD               - run the build, create MicroPython firmware
# ./BUILD -j4           - run the build on multicore system, much faster build
#                         replace 4 with the number of cores on your system
# ./BUILD menuconfig    - run menuconfig to configure ESP32/MicroPython
# ./BUILD clean         - clean the build
# ./BUILD flash         - flash MicroPython firmware to ESP32
# ./BUILD erase         - erase the whole ESP32 Flash
# ./BUILD monitor       - run esp-idf terminal program



#---------------------------------------------------------------------------------------------------------------------
# Check parameters
opt="$1"
opt2="xx"
arg="$1"
buildType="esp2"

if [ "${opt:0:2}" == "-j" ]; then
    opt2="$2"
    if [ "${opt2}" == "" ]; then
        opt2="all"
    fi
    if [ "${opt2}" == "psram" ]; then
        opt2="all"
        buildType="psram"
    fi
    if [ "${opt2}" != "flash" ] && [ "${opt2}" != "erase" ] && [ "${opt2}" != "monitor" ] && [ "${opt2}" != "clean" ] && [ "${opt2}" != "all" ] && [ "${opt2}" != "menuconfig" ]; then
        echo ""
        echo "Wrong parameter, usage: BUILD.sh [all] | clean | flash | erase | monitor | menuconfig"
        echo ""
        exit 1
    else
        arg=${opt2}
        if [ "$3" == "psram" ]; then
            buildType="psram"
        fi
    fi
else
    if [ "${opt}" == "" ]; then
        opt="all"
    fi
    if [ "${opt}" != "flash" ] && [ "${opt}" != "erase" ] && [ "${opt}" != "monitor" ] && [ "${opt}" != "clean" ] && [ "${opt}" != "all" ] && [ "${opt}" != "menuconfig" ]; then
        echo ""
        echo "Wrong parameter, usage: BUILD.sh [all] | clean | flash | erase | monitor | menuconfig"
        exit 1
        echo ""
    fi
    opt=""
    if [ "$2" == "psram" ]; then
        buildType="psram"
    fi
fi

if [ "${arg}" == "" ]; then
    arg="all"
fi
#---------------------------------------------------------------------------------------------------------------------



BUILD_BASE_DIR=${PWD}

# ########################################################
# #              SET XTENSA & ESP-IDF PATHS              #
# ########################################################
if [ "${buildType}" == "psram" ]; then
    cd ../
    XTENSA_DIR=${PWD}

    cd ${BUILD_BASE_DIR}

    # Add Xtensa toolchain path to system path
    export PATH=${XTENSA_DIR}/xtensa-esp32-elf_psram/bin:$PATH
    # Export esp-idf path
    export IDF_PATH=${XTENSA_DIR}/esp-idf_psram

    echo ""
    echo "Building MicroPython for ESP32 with esp-idf psRAM branch"
    echo ""
else
    # ############################################################
    # !PUT HERE THE REAL PATHS TO YOUR XTENSA TOOLCHAIN & ESP-IDF!
    # vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    export PATH=$PATH:/home/LoBo2_Razno/ESP32/xtensa-esp32-elf/bin
    export IDF_PATH=/home/LoBo2_Razno/ESP32/esp-idf
    # ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    # ############################################################

    echo ""
    echo "Building MicroPython for ESP32 with esp-idf master branch"
    echo ""
fi

export HOST_PLATFORM=linux
export CROSS_COMPILE=xtensa-esp32-elf-

# ########################################################



# Test if mpy-cross has to be buildType
# ----------------------------------------------------------------------------------------------------------------------
if [ "${arg}" != "flash" ] && [ "${arg}" != "erase" ] && [ "${arg}" != "monitor" ] && [ "${arg}" != "menuconfig" ]; then
    # ###########################################################################
    # Build MicroPython cross compiler which compiles .py scripts into .mpy files
    # ###########################################################################
    if [ ! -f "components/micropython/mpy-cross/mpy-cross" ]; then
        echo "=================="
        echo "Building mpy-cross"
        make -C components/micropython/mpy-cross > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo "OK."
            echo "=================="
        else
            echo "FAILED"
            echo "=================="
            exit 1
        fi
    fi
    # #########################################
fi

# #### Test sdkconfig ###############
if [ "${arg}" != "flash" ] && [ "${arg}" != "erase" ] && [ "${arg}" != "monitor" ] && [ "${arg}" != "menuconfig" ]; then
    # Test if sdkconfig exists
    # ---------------------------
    if [ ! -f "sdkconfig" ]; then
        echo "sdkconfig NOT FOUND, RUN ./BUILD.sh menuconfig FIRST."
        echo ""
        exit 1
    fi

    # Test if toolchain is changed
    SDK_PSRAM=$(grep -e CONFIG_MEMMAP_SPIRAM_ENABLE sdkconfig)
    if [ "${buildType}" == "psram" ] && [ "${SDK_PSRAM}" == "" ]; then
        echo "TOOLCHAIN CHANGED, RUN"
        echo "./BUILD.sh menuconfig psram"
        echo "./BUILD.sh clean psram"
        esho "TO BUILD WITH psRAM support."
        echo ""
        exit 1
    fi

    if [ ! "${buildType}" == "psram" ] && [ "${SDK_PSRAM}" != "" ]; then
        echo "TOOLCHAIN CHANGED, RUN"
        echo "./BUILD.sh menuconfig"
        echo "./BUILD.sh clean"
        esho "TO BUILD WITHOUT psRAM support."
        echo ""
        exit 1
    fi
fi
# ###################################


if [ "${arg}" == "flash" ]; then
    echo "========================================="
    echo "Flashing MicroPython firmware to ESP32..."
    echo "========================================="

    make  ${opt} ${arg}
elif [ "${arg}" == "erase" ]; then
    echo "======================"
    echo "Erasing ESP32 Flash..."
    echo "======================"

    make  ${opt} ${arg}
elif [ "${arg}" == "monitor" ]; then
    echo "============================"
    echo "Executing esp-idf monitor..."
    echo "============================"

    make  ${opt} ${arg}
elif [ "${arg}" == "clean" ]; then
    echo "============================="
    echo "Cleaning MicroPython build..."
    echo "============================="

    make  ${opt} ${arg} > /dev/null 2>&1
elif [ "${arg}" == "menuconfig" ]; then
    make  ${opt} ${arg}
else
    echo "================================"
    echo "Building MicroPython firmware..."
    echo "================================"

    make  ${opt} ${arg} > /dev/null 2>&1
fi

if [ $? -eq 0 ]; then
    echo "OK."
    if [ "${arg}" == "all" ]; then
        if [ "${buildType}" == "psram" ]; then
            cp -f build/MicroPython.bin firmware/esp32_psram > /dev/null 2>&1
            cp -f build/bootloader/bootloader.bin firmware/esp32_psram/bootloader > /dev/null 2>&1
            cp -f build/partitions_singleapp.bin firmware/esp32_psram > /dev/null 2>&1
            echo "#!/bin/bash" > firmware/esp32_psram/flash.sh
            make print_flash_cmd >> firmware/esp32_psram/flash.sh
            chmod +x firmware/esp32_psram/flash.sh > /dev/null 2>&1
        else
            cp -f build/MicroPython.bin firmware/esp32 > /dev/null 2>&1
            cp -f build/bootloader/bootloader.bin firmware/esp32/bootloader > /dev/null 2>&1
            cp -f build/partitions_singleapp.bin firmware/esp32 > /dev/null 2>&1
            echo "#!/bin/bash" > firmware/esp32/flash.sh
            make print_flash_cmd >> firmware/esp32/flash.sh
            chmod +x firmware/esp32/flash.sh > /dev/null 2>&1
        fi
        echo "Build complete."
        echo "You can now run ./BUILD.sh flash"
        echo "to deploy the firmware to ESP32"
        echo "--------------------------------"
    fi
    echo ""
else
    echo "Build FAILED!"
    echo ""
    exit 1
fi

