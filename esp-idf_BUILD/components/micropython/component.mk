#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

CFLAGS += -DFFCONF_H=\"lib/oofatfs/ffconf.h\"

#COMPONENT_SRCDIRS := . py esp32 lib extmod lib/utils lib/lwip/src lib/netutils ../
COMPONENT_SRCDIRS := . py esp32 lib lib/utils extmod lib/mp-readline extmod/crypto-algorithms lib/netutils drivers/dht lib/oofatfs lib/timeutils lib/oofatfs/option ../

#COMPONENT_ADD_INCLUDEDIRS := . py esp32 lib extmod lib/utils lib/netutils lib/lwip/src/include lib/lwip/src/include/ipv4 lib/lwip/src/include/lwip extmod/lwip-include ../
COMPONENT_ADD_INCLUDEDIRS := . py esp32 lib lib/utils lib/mp-readline extmod extmod/crypto-algorithms ../genhdr lib/netutils drivers/dht lib/oofatfs lib/timeutils lib/oofatfs/option ../

COMPONENT_PRIV_INCLUDEDIRS:=py esp32 lib extmod ../genhdr ../
