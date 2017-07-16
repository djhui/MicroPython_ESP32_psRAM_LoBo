#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := main

CROSS_COMPILE ?= xtensa-esp32-elf-
COMPONENTS_ROOT = $(PWD)/components
MICROPY_ROOT ?= $(COMPONENTS_ROOT)/micropython
BUILD ?= $(COMPONENTS_ROOT)/../build

#include $(MICROPY_ROOT)/py/mkenv.mk

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

MICROPY_PY_USSL = 0
MICROPY_SSL_AXTLS = 0
MICROPY_FATFS = 1
MICROPY_PY_BTREE = 0

#FROZEN_DIR = scripts
FROZEN_MPY_DIR = $(COMPONENTS_ROOT)/micropython/esp32/modules

# include py core make definitions
#include $(MICROPY_ROOT)/py/py.mk


#include $(MICROPY_ROOT)/py/mkrules.mk

include $(IDF_PATH)/make/project.mk
