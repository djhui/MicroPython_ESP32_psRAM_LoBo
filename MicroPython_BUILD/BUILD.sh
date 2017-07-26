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
    if [ "${opt2}" == "psram" ]; then
        opt2="all"
        buildType="psram"
    fi
    if [ "${opt2}" == "" ]; then
        opt2="all"
    fi
    if [ "${opt2}" != "flash" ] && [ "${opt2}" != "all" ]; then
        echo ""
        echo "Wrong parameter, usage: BUILD.sh -jN [all] | flash"
        echo ""
        exit 1
    else
        arg=${opt2}
        if [ "$3" == "psram" ]; then
            buildType="psram"
        fi
    fi

elif [ "${opt}" == "makefs" ] || [ "${opt}" == "flashfs" ] || [ "${opt}" == "copyfs" ]; then
    arg=${opt}
    opt=""
    
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

# Test if toolchains are unpacked
# #########################################################
cd ..
if [ ! -d "esp-idf" ]; then
    echo "unpacking 'esp-idf'"
    tar -xf esp-idf.tar.xz> /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "unpacking 'esp-idf' FAILED"
        exit 1
    fi
fi
if [ ! -d "esp-idf_psram" ]; then
    echo "unpacking 'esp-idf_psram'"
    tar -xf esp-idf_psram.tar.xz> /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "unpacking 'esp-idf_psram' FAILED"
        exit 1
    fi
fi
if [ ! -d "xtensa-esp32-elf" ]; then
    echo "unpacking 'xtensa-esp32-elf'"
    tar -xf xtensa-esp32-elf.tar.xz> /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "unpacking 'xtensa-esp32-elf' FAILED"
        exit 1
    fi
fi
if [ ! -d "xtensa-esp32-elf_psram" ]; then
    echo "unpacking 'xtensa-esp32-elf_psram'"
    tar -xf xtensa-esp32-elf_psram.tar.xz> /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "unpacking 'xtensa-esp32-elf_psram' FAILED"
        exit 1
    fi
fi
cd ${BUILD_BASE_DIR}
# #########################################################


# Test if mpy-cross has to be build
# ----------------------------
if [ "${arg}" == "all" ]; then
    # ###########################################################################
    # Build MicroPython cross compiler which compiles .py scripts into .mpy files
    # ###########################################################################
    if [ ! -f "components/micropython/mpy-cross/mpy-cross" ]; then
        cd components/mpy_cross_build/mpy-cross
        echo "=================="
        echo "Building mpy-cross"
        make > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            cp -f mpy-cross ../../micropython/mpy-cross > /dev/null 2>&1
            make clean > /dev/null 2>&1
            echo "OK."
            echo "=================="
        else
            echo "FAILED"
            echo "=================="
            exit 1
        fi
        cd ${BUILD_BASE_DIR}
    fi
    # ###########################################################################
fi

# ----------------------------------------------------------------------------------------
if [ "${arg}" == "makefs" ] || [ "${arg}" == "flashfs" ] || [ "${arg}" == "copyfs" ]; then
    # ###########################################################################
    # Build mkspiffs program which creates spiffs image from directory
    # ###########################################################################
    if [ ! -f "components/mkspiffs/src/mkspiffs" ]; then
        echo "=================="
        echo "Building mkspiffs"
        make -C components/mkspiffs/src > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            echo "OK."
            echo "=================="
        else
            echo "FAILED"
            echo "=================="
            exit 1
        fi
    fi
    # ###########################################################################
fi


# ########################################################
# #              SET XTENSA & ESP-IDF PATHS              #
# ########################################################
if [ "${buildType}" == "psram" ]; then
    cd ../
    # Add Xtensa toolchain path to system path
    export PATH=${PWD}/xtensa-esp32-elf_psram/bin:$PATH
    # Export esp-idf path
    export IDF_PATH=${PWD}/esp-idf_psram

    cd ${BUILD_BASE_DIR}
    echo ""
    echo "Building MicroPython for ESP32 with esp-idf psRAM branch"
    echo ""
else
    cd ../
    # Add Xtensa toolchain path to system path
    export PATH=${PWD}/xtensa-esp32-elf/bin:$PATH
    # Export esp-idf path
    export IDF_PATH=${PWD}/esp-idf

    # ############################################################
    # !PUT HERE THE REAL PATHS TO YOUR XTENSA TOOLCHAIN & ESP-IDF!
    # vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    #export PATH=$PATH:/home/LoBo2_Razno/ESP32/xtensa-esp32-elf/bin
    #export IDF_PATH=/home/LoBo2_Razno/ESP32/esp-idf_mpy
    # ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    # fcdc5ccdfac927f7ba919c661519de70d44c3493  11/07 last working
    # ############################################################

    cd ${BUILD_BASE_DIR}
    echo ""
    echo "Building MicroPython for ESP32 with esp-idf master branch"
    echo ""
fi

export HOST_PLATFORM=linux
export CROSS_COMPILE=xtensa-esp32-elf-

# ########################################################



# #### Test sdkconfig ###############
if [ "${arg}" == "all" ] || [ "${arg}" == "clean" ]; then
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
        echo "TO BUILD WITH psRAM support."
        echo ""
        exit 1
    fi

    if [ ! "${buildType}" == "psram" ] && [ "${SDK_PSRAM}" != "" ]; then
        echo "TOOLCHAIN CHANGED, RUN"
        echo "./BUILD.sh menuconfig"
        echo "./BUILD.sh clean"
        echo "TO BUILD WITHOUT psRAM support."
        echo ""
        exit 1
    fi
fi
# ###################################


if [ "${arg}" == "flash" ]; then
    echo "========================================="
    echo "Flashing MicroPython firmware to ESP32..."
    echo "========================================="

    make ${opt} ${arg}
elif [ "${arg}" == "erase" ]; then
    echo "======================"
    echo "Erasing ESP32 Flash..."
    echo "======================"

    make erase_flash
elif [ "${arg}" == "monitor" ]; then
    echo "============================"
    echo "Executing esp-idf monitor..."
    echo "============================"

    make monitor
elif [ "${arg}" == "clean" ]; then
    echo "============================="
    echo "Cleaning MicroPython build..."
    echo "============================="

    rm -f components/micropython/mpy-cross/mpy-cross > /dev/null 2>&1
    rm -f components/mkspiffs/src/*.o > /dev/null 2>&1
    rm -f components/mkspiffs/src/spiffs/*.o > /dev/null 2>&1
    if [ "${MP_SHOW_PROGRESS}" == "yes" ]; then
        make clean
    else
        make clean > /dev/null 2>&1
    fi
elif [ "${arg}" == "menuconfig" ]; then
    make menuconfig
elif [ "${arg}" == "makefs" ]; then
    echo "========================"
    echo "Creating SPIFFS image..."
    echo "========================"
    make makefs
elif [ "${arg}" == "flashfs" ]; then
    echo "================================="
    echo "Flashing SPIFFS image to ESP32..."
    echo "================================="
    make flashfs
elif [ "${arg}" == "copyfs" ]; then
    echo "========================================="
    echo "Flashing default SPIFFS image to ESP32..."
    echo "========================================="
    make copyfs
else
    echo "================================"
    echo "Building MicroPython firmware..."
    echo "================================"

    if [ "${MP_SHOW_PROGRESS}" == "yes" ]; then
        make  ${opt} ${arg}
    else
        make  ${opt} ${arg} > /dev/null 2>&1
    fi
fi

if [ $? -eq 0 ]; then
    echo "OK."
    if [ "${arg}" == "all" ]; then
        if [ "${buildType}" == "psram" ]; then
            cp -f build/MicroPython.bin firmware/esp32_psram > /dev/null 2>&1
            cp -f build/bootloader/bootloader.bin firmware/esp32_psram/bootloader > /dev/null 2>&1
            cp -f build/partitions_singleapp.bin firmware/esp32_psram > /dev/null 2>&1
            cp -f sdkconfig firmware/esp32_psram > /dev/null 2>&1
            echo "#!/bin/bash" > firmware/esp32_psram/flash.sh
            make print_flash_cmd >> firmware/esp32_psram/flash.sh
            chmod +x firmware/esp32_psram/flash.sh > /dev/null 2>&1
        else
            cp -f build/MicroPython.bin firmware/esp32 > /dev/null 2>&1
            cp -f build/bootloader/bootloader.bin firmware/esp32/bootloader > /dev/null 2>&1
            cp -f build/partitions_singleapp.bin firmware/esp32 > /dev/null 2>&1
            cp -f sdkconfig firmware/esp32 > /dev/null 2>&1
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
    echo "'make ${arg}' FAILED!"
    echo ""
    exit 1
fi

