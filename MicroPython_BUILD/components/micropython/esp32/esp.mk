# Includes for Qstr&Frozen modules
#---------------------------------
MP_EXTRA_INC += -I.
MP_EXTRA_INC += -I$(PROJECT_PATH)/main
MP_EXTRA_INC += -I$(COMPONENT_PATH)
MP_EXTRA_INC += -I$(PROJECT_PATH)/components/spiffs
MP_EXTRA_INC += -I$(COMPONENT_PATH)/py
MP_EXTRA_INC += -I$(COMPONENT_PATH)/lib/mp-readline
MP_EXTRA_INC += -I$(COMPONENT_PATH)/lib/netutils
MP_EXTRA_INC += -I$(COMPONENT_PATH)/lib/timeutils
MP_EXTRA_INC += -I$(COMPONENT_PATH)/esp32
MP_EXTRA_INC += -I$(COMPONENT_PATH)/build
MP_EXTRA_INC += -I$(COMPONENT_PATH)/esp32/libs
MP_EXTRA_INC += -I$(BUILD_DIR_BASE)
MP_EXTRA_INC += -I$(BUILD_DIR_BASE)/include
#MP_EXTRA_INC += -I$(ESPCOMP)/bootloader_support/include
MP_EXTRA_INC += -I$(ESPCOMP)/driver/include
MP_EXTRA_INC += -I$(ESPCOMP)/driver/include/driver
MP_EXTRA_INC += -I$(ESPCOMP)/nghttp/port/include
MP_EXTRA_INC += -I$(ESPCOMP)/nghttp/nghttp2/lib/includes
MP_EXTRA_INC += -I$(ESPCOMP)/esp32/include
MP_EXTRA_INC += -I$(ESPCOMP)/soc/esp32/include
MP_EXTRA_INC += -I$(ESPCOMP)/ethernet/include
MP_EXTRA_INC += -I$(ESPCOMP)/expat/include/expat
MP_EXTRA_INC += -I$(ESPCOMP)/expat/port/include
MP_EXTRA_INC += -I$(ESPCOMP)/json/include
MP_EXTRA_INC += -I$(ESPCOMP)/json/port/include
MP_EXTRA_INC += -I$(ESPCOMP)/log/include
MP_EXTRA_INC += -I$(ESPCOMP)/newlib/include
MP_EXTRA_INC += -I$(ESPCOMP)/nvs_flash/include
MP_EXTRA_INC += -I$(ESPCOMP)/freertos/include
MP_EXTRA_INC += -I$(ESPCOMP)/tcpip_adapter/include
MP_EXTRA_INC += -I$(ESPCOMP)/lwip/include/lwip
MP_EXTRA_INC += -I$(ESPCOMP)/lwip/include/lwip/port
MP_EXTRA_INC += -I$(ESPCOMP)/lwip/include/lwip/posix
MP_EXTRA_INC += -I$(ESPCOMP)/mbedtls/include
MP_EXTRA_INC += -I$(ESPCOMP)/mbedtls/port/include
MP_EXTRA_INC += -I$(ESPCOMP)/spi_flash/include
MP_EXTRA_INC += -I$(ESPCOMP)/wear_levelling/include
MP_EXTRA_INC += -I$(ESPCOMP)/wear_levelling/private_include
MP_EXTRA_INC += -I$(ESPCOMP)/vfs/include
MP_EXTRA_INC += -I$(ESPCOMP)/newlib/platform_include
MP_EXTRA_INC += -I$(ESPCOMP)/xtensa-debug-module/include
MP_EXTRA_INC += -I$(ESPCOMP)/wpa_supplicant/include
MP_EXTRA_INC += -I$(ESPCOMP)/wpa_supplicant/port/include
MP_EXTRA_INC += -I$(ESPCOMP)/ethernet/include
MP_EXTRA_INC += -I$(ESPCOMP)/app_trace/include
MP_EXTRA_INC += -I$(ESPCOMP)/sdmmc/include
MP_EXTRA_INC += -I$(ESPCOMP)/fatfs/src
MP_EXTRA_INC += -I$(ESPCOMP)/heap/include

# Clean MicroPython directories/files
# -----------------------------------
MP_CLEAN_EXTRA = $(BUILD_DIR_BASE)/drivers
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/esp32
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/extmod
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/frozen_mpy
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/genhdr
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/home
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/lib
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/micropython/*
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/py
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/frozen_mpy.c
MP_CLEAN_EXTRA += $(BUILD_DIR_BASE)/frozen.c
MP_CLEAN_EXTRA += $(COMPONENT_PATH)/genhdr/qstrdefs.generated.h

# List of MicroPython source and object files
# for Qstr generation
# -------------------------------------------
SRC_C =  $(addprefix esp32/,\
	uart.c \
	gccollect.c \
	mphalport.c \
	fatfs_port.c \
	help.c \
	esponewire.c \
	modutime.c \
	moduos.c \
	machine_timer.c \
	machine_pin.c \
	machine_touchpad.c \
	machine_adc.c \
	machine_dac.c \
	machine_pwm.c \
	machine_uart.c \
	modmachine.c \
	modnetwork.c \
	modsocket.c \
	modesp.c \
	moduhashlib.c \
	espneopixel.c \
	machine_hw_spi.c \
	mpthreadport.c \
	mpsleep.c \
	machrtc.c \
	)

EXTMOD_SRC_C = $(addprefix extmod/,\
	modbtree.c \
	)

LIB_SRC_C = $(addprefix lib/,\
	libm/math.c \
	libm/fmodf.c \
	libm/roundf.c \
	libm/ef_sqrt.c \
	libm/kf_rem_pio2.c \
	libm/kf_sin.c \
	libm/kf_cos.c \
	libm/kf_tan.c \
	libm/ef_rem_pio2.c \
	libm/sf_sin.c \
	libm/sf_cos.c \
	libm/sf_tan.c \
	libm/sf_frexp.c \
	libm/sf_modf.c \
	libm/sf_ldexp.c \
	libm/asinfacosf.c \
	libm/atanf.c \
	libm/atan2f.c \
	mp-readline/readline.c \
	netutils/netutils.c \
	timeutils/timeutils.c \
	utils/pyexec.c \
	utils/interrupt_char.c \
	utils/sys_stdio_mphal.c \
	)

LIBS_SPIFFS_C = $(addprefix $(PROJECT_PATH)/components/spiffs/,\
	esp_spiffs.c \
	list.c \
	mutex.c \
	spiffs_cache.c \
	spiffs_check.c \
	spiffs_gc.c \
	spiffs_hydrogen.c \
	spiffs_nucleus.c \
	spiffs_vfs.c \
	)

LIBS_SRC_C = $(addprefix esp32/libs/,\
	modonewire.c \
	)

LIB_SRC_C += \
	$(ESPCOMP)/fatfs/src/ff.c \
	$(ESPCOMP)/fatfs/src/diskio_sdmmc.c \
	$(ESPCOMP)/fatfs/src/diskio_spiflash.c \
	$(ESPCOMP)/fatfs/src/diskio.c \
	$(ESPCOMP)/fatfs/src/vfs_fat_sdmmc.c \
	$(ESPCOMP)/fatfs/src/vfs_fat_spiflash.c \
	$(ESPCOMP)/fatfs/src/vfs_fat.c \
	$(ESPCOMP)/fatfs/src/option/syscall.c \
	$(ESPCOMP)/fatfs/src/option/unicode.c

ifeq ($(MICROPY_PY_BTREE),1)
LIB_SRC_C += \
	lib/berkeley-db-1.xx/btree/bt_open.c \
	lib/berkeley-db-1.xx/btree/bt_seq.c \
	lib/berkeley-db-1.xx/btree/bt_close.c \
	lib/berkeley-db-1.xx/btree/bt_debug.c \
	lib/berkeley-db-1.xx/btree/bt_get.c \
	lib/berkeley-db-1.xx/btree/bt_overflow.c \
	lib/berkeley-db-1.xx/btree/bt_put.c \
	lib/berkeley-db-1.xx/btree/bt_utils.c \
	lib/berkeley-db-1.xx/btree/bt_conv.c \
	lib/berkeley-db-1.xx/btree/bt_delete.c \
	lib/berkeley-db-1.xx/btree/bt_page.c \
	lib/berkeley-db-1.xx/btree/bt_search.c \
	lib/berkeley-db-1.xx/btree/bt_split.c \
	lib/berkeley-db-1.xx/mpool/mpool.c
endif

DRIVERS_SRC_C = $(addprefix drivers/,\
	dht/dht.c \
	)

OBJ_MP =
OBJ_MP += $(PY_O)
OBJ_MP += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(EXTMOD_SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(LIB_SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(LIBS_SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(LIBS_SPIFFS_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(DRIVERS_SRC_C:.c=.o))

