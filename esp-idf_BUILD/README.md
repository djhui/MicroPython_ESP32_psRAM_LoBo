# MicroPython for ESP32-WROVER with 4MB of psRAM

---

This directory contains minimal MicroPython building environment, which uses **esp-idf way** of building applications.

It is mostly used to create **sdkconfig.h** which can be copied to the main ESP32 directory and used for main MicroPython build.

You can change any configuration option, but keep two options under **→ Component config → FreeRTOS** enabled:

* Enable FreeRTOS static allocation API
* Enable static task clean up hook



```

./BUILD.sh menuconfig
./BUILD.sh [-jN]


```

**sdkconfig.h** is created in build/include directory. You can copy it to the main esp32 directory to be used in main MicroPython build.

---

The created firmware can be flashed and runs, but it contains only the minimum system without network support or file system.
