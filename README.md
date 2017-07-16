# MicroPython for ESP32-WROVER with 4MB of psRAM

---

MicroPython works great on ESP32, but the most serious issue is still (as on most other MicroPython boards) limited amount of free memory.

ESP32 can use external **SPI RAM (psRAM)** to expand available RAM up to 16MB. Currently, there is only one module which incorporates **4MB** of psRAM, the **ESP-WROVER module**

It is hard to get, but it is available on some **ESP-WROVER-KIT boards** (the one on which this build was tested on).

[Pycom](https://www.pycom.io/webshop) is also offering the boards and OEM modules with 4MB of psRAM, to be available in August/September.

---

This repository contains all the tools and sources necessary to **build working MicroPython firmware** which can fully use the advantages of **4MB** (or more) of **psRAM**

It is **huge difference** between MicroPython running with **less than 100KB** of free memory and running with **4MB** of free memory.

### Features

* MicroPython build is added as **submodule** to [main Micropython repository](https://github.com/micropython/micropython)
* ESP32 build is based on [MicroPython's ESP32 build](https://github.com/micropython/micropython-esp32/tree/esp32/esp32) with added changes needed to build on ESP32 with psRAM
* Special [esp-idf branch](https://github.com/espressif/esp-idf/tree/feature/psram_malloc) is used, with some modifications needed to build MicroPython
* Special Xtenssa ESP32 toolchain is needed for building psRAM enabled application. It is included in this repository.
* Default configuration has **4MB** of MicroPython heap, **64KB** of MicroPython stack, **~200KB** of free DRAM heap for C modules and functions
* MicroPython can be built in **unicore** (FreeRTOS & MicroPython task running only on the first ESP32 core, or **dualcore** configuration (MicroPython task running on ESP32 **App** core
* ESP32 Flash can be configured in any mode, **QIO**, **QOUT**, **DIO**, **DOUT**
* Special build directory is provided to create **sdkconfig.h** wid desired configuration
* **BUILD.sh** script is provided to make **building** MicroPython firmware as **easy** as possible
* Internal filesystem is built with esp-idf **wear leveling** driver, so there is less danger of damaging the flash with frequent writes
* **sdcard** module is included which uses esp-idf **sdmmc** driver and can work in **1-bit** and **4-bit** modes. On ESP32-WROVER-KIT it works without changes, for imformation on how to connect sdcard on other boards look at *esp32/modesp.c*
* **RTC Class** is added to machine module, including methods for synchronization of system time to **ntp** server, **deepsleep**, **wakeup** from deepsleep **on external pin** level, ...
* files **timestamp** is correctly set to system time both on internal fat filesysten and on sdcard
* Some additional frozen modules are added, like **pye** editor, **upip**, **urequests**, ...


There are some new configuration options in **mpconfigport.h** you may want to change:
```
// internal flash file system configuration
#define MICROPY_INTERNALFS_START            (0x180000)  // filesystem start Flash address
#define MICROPY_INTERNALFS_SIZE             (0x200000)  // size of the Flash used for filesystem
#define MICROPY_INTERNALFS_ENCRIPTED        (0)         // use encription on filesystem (UNTESTED!)

// === sdcard using ESP32 sdmmc driver configuration ===
#define MICROPY_SDMMC_SHOW_INFO             (1)         // show sdcard info after initialization if set to 1

// Set the time zone string used on synchronization with NTP server
// Comment if not used
#define MICROPY_TIMEZONE "CET-1CEST"
```

There are few prepared **sdkconfig.h** configuration files you can use. You can copy any of them to sdkconfig.h

The default is dualcore, DIO.

* **sdkconfig.h.dio**  dual core, DIO Flash mode
* **sdkconfig.h.qio**  dual core, QIO Flash mode
* **sdkconfig.h.unicore**  one core, QIO Flash mode
* **sdkconfig.h.nopsram**  does not use psRAM, can be used to build for **any** ESP32 module

---

### How to Build

Clone this repository, as it uses submodules, use --recursive option

```
git clone --recursive https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo.git
```
You have to extract Xtenssa toolchain from archive. Goto *MicroPython_ESP32_psRAM_LoBo* directory and execute:
```
tar -xf xtensa-esp32-elf_psram.tar.xz
```
Edit **BUILD.sh** script, and change if necessary, **PORT** and **BOUD** options. Leave all other options as is.

You can also go to the **esp-idf_BUILD** build directory to create your own **sdkconfig.h**

To build the MicroPython firmware, run:
```
./BUILD.sh
```
You can use -jN (N=number of cores to use) to make the build process faster (it only takes ~10 seconds on my system with -j8).

If no errors are detected, you can now flash the MicroPython firmware to your board. Run:
```
./BUILD.sh deploy
```
The board stays in bootloader mode. Run your terminal emulator and reset the board.

You can also run *./BUILD.sh monitor* to use esp-idf's terminal program, it will reset the boars automatically.

---

To update the repository, run
```
git pull
```

You can also update the submodules, run
```
git submodule update --init --recursive
```

---

### Some examples

Using new machine methods and RTC:
```
import machine

rtc = machine.RTC()

rtc.init((2017, 6, 12, 14, 35, 20))

rtc.now()

rtc.ntp_sync(server="<ntp_server>" [,update_period=])
  <ntp_server> can be empty string, then the default server is used ("pool.ntp.org")

rtc.synced()
  returns True if time synchronized to NTP server

rtc.wake_on_ext0(Pin, level)
rtc.wake_on_ext1(Pin, level)
  wake up from deepsleep on pin level

machine.deepsleep(time_ms)
machine.wake_reason()
  returns tuple with reset & wakeup reasons
machine.wake_description()
  returns tuple with strings describing reset & wakeup reasons


```

Using sdcard module:
```
import sdcard, uos, esp

sd = sdcard.SDCard(esp.SD_1LINE)
vfs = uos.VfsFat(sd)
uos.mount(vfs, '/sd')
uos.chdir('/sd')
uos.listdir()
```

Using sdcard module with automount:
```
>>> import sdcard,esp,uos
>>> sd = sdcard.SDCard(esp.SD_4LINE, True)
---------------------
Initializing SD Card: OK.
---------------------
 Mode:  SD (4bit)
 Name: SL08G
 Type: SDHC/SDXC
Speed: default speed (25 MHz)
 Size: 7580 MB
  CSD: ver=1, sector_size=512, capacity=15523840 read_bl_len=9
  SCR: sd_spec=2, bus_width=5

>>> uos.listdir()
['overlays', 'bcm2708-rpi-0-w.dtb', ......
>>>

```

---

### Example terminal session


```

I (47) boot: ESP-IDF  2nd stage bootloader
I (47) boot: compile time 21:04:46
I (69) boot: Enabling RNG early entropy source...
I (69) qio_mode: Enabling QIO for flash chip GD
I (75) boot: SPI Speed      : 40MHz
I (88) boot: SPI Mode       : QIO
I (100) boot: SPI Flash Size : 4MB
I (113) boot: Partition Table:
I (124) boot: ## Label            Usage          Type ST Offset   Length
I (147) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (170) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (193) boot:  2 factory          factory app      00 00 00010000 00100000
I (217) boot: End of partition table
I (230) boot: Disabling RNG early entropy source...
I (247) boot: Loading app partition at offset 00010000
I (1440) boot: segment 0: paddr=0x00010018 vaddr=0x00000000 size=0x0ffe8 ( 65512) 
I (1440) boot: segment 1: paddr=0x00020008 vaddr=0x3f400010 size=0x30c50 (199760) map
I (1457) boot: segment 2: paddr=0x00050c60 vaddr=0x3ffb0000 size=0x04204 ( 16900) load
I (1487) boot: segment 3: paddr=0x00054e6c vaddr=0x40080000 size=0x00400 (  1024) load
I (1510) boot: segment 4: paddr=0x00055274 vaddr=0x40080400 size=0x15050 ( 86096) load
I (1563) boot: segment 5: paddr=0x0006a2cc vaddr=0x400c0000 size=0x00074 (   116) load
I (1564) boot: segment 6: paddr=0x0006a348 vaddr=0x00000000 size=0x05cc0 ( 23744) 
I (1588) boot: segment 7: paddr=0x00070010 vaddr=0x400d0018 size=0xa25c0 (665024) map
I (1614) boot: segment 8: paddr=0x001125d8 vaddr=0x50000000 size=0x00008 (     8) load
I (1642) cpu_start: PSRAM mode: flash 40m sram 40m
I (1657) cpu_start: PSRAM initialized, cache is in even/odd (2-core) mode.
I (1680) cpu_start: Pro cpu up.
I (1692) cpu_start: Starting app cpu, entry point is 0x4008237c
I (0) cpu_start: App cpu up.
I (4341) heap_alloc_caps: SPI SRAM memory test OK
I (4341) heap_alloc_caps: Initializing. RAM available for dynamic allocation:
I (4347) heap_alloc_caps: At 3F800000 len 00400000 (4096 KiB): SPIRAM
I (4369) heap_alloc_caps: At 3FFAE2A0 len 00001D60 (7 KiB): DRAM
I (4389) heap_alloc_caps: At 3FFBA310 len 00025CF0 (151 KiB): DRAM
I (4410) heap_alloc_caps: At 3FFE0440 len 00003BC0 (14 KiB): D/IRAM
I (4432) heap_alloc_caps: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (4453) heap_alloc_caps: At 40095450 len 0000ABB0 (42 KiB): IRAM
I (4474) cpu_start: Pro cpu start user code
I (4533) cpu_start: Starting scheduler on PRO CPU.
I (2828) cpu_start: Starting scheduler on APP CPU.

FreeRTOS running on BOTH CORES, MicroPython task started on App Core.

Allocating uPY stack: size=65536 bytes
Allocating uPY heap:  size=4194048 bytes (in SPIRAM using pvPortMallocCaps)

Reset reason: Power on reset Wakeup: Power on wake
I (3138) phy: phy_version: 350, Mar 22 2017, 15:02:06, 0, 2

Starting WiFi ...
WiFi started
Synchronize time from NTP server ...
Time set
19:5:35 14/7/2017

MicroPython c3fd0cf-dirty on 2017-07-14; ESP-WROVER module with ESP32+psRAM
Type "help()" for more information.
>>> 
>>> import micropython
>>> micropython.mem_info()
stack: 736 out of 64512
GC: total: 4097984, used: 11200, free: 4086784
 No. of 1-blocks: 117, 2-blocks: 14, max blk sz: 264, max free sz: 255416
>>> 
>>> a = ['esp32'] * 200000
>>> 
>>> a[123456]
'esp32'
>>> 
>>> micropython.mem_info()
stack: 736 out of 64512
GC: total: 4097984, used: 811664, free: 3286320
 No. of 1-blocks: 133, 2-blocks: 19, max blk sz: 50000, max free sz: 205385
>>> 

```
