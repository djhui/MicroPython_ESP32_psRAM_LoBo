# MicroPython for ESP32-WROVER with 4MB of psRAM

---

Before building extract Xtensa toolchain from archive, then build using *BUILD.sh* script:


```

tar -xf xtensa-esp32-elf_psram.tar.xz

./BUILD.sh

or

./BUILD.sh -j8

then flash:

./BUILD.sh deploy


```

---


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
