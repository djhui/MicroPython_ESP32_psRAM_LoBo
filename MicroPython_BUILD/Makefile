#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := MicroPython

ESPTOOLPY_SERIAL = $(IDF_PATH)/components/esptool_py/esptool/esptool.py --chip esp32 --port $(CONFIG_ESPTOOLPY_PORT) --baud $(CONFIG_ESPTOOLPY_BAUD) --before $(CONFIG_ESPTOOLPY_BEFORE) --after $(CONFIG_ESPTOOLPY_AFTER)

print_flash_cmd:
	echo $(ESPTOOLPY_SERIAL) write_flash $(if $(CONFIG_ESPTOOLPY_COMPRESSED),-z,-u) $(ESPTOOL_WRITE_FLASH_OPTIONS) $(ESPTOOL_ALL_FLASH_ARGS) | sed -e 's:'$(PWD)/build/'::g'

include $(IDF_PATH)/make/project.mk
