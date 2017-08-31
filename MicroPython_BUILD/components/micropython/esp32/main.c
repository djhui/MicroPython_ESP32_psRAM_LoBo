/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
 *
 * Copyright (c) 2017 Boris Lovosevic (External SPIRAM support)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if IDF_USEHEAP
#include "esp_heap_caps.h"
#else
#include "esp_heap_alloc_caps.h"
#endif
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task.h"
#include "soc/cpu.h"
#include "esp_log.h"

#include "py/stackctrl.h"
#include "py/nlr.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "lib/mp-readline/readline.h"
#include "lib/utils/pyexec.h"
#include "uart.h"
#include "modmachine.h"
#include "mpthreadport.h"
#include "mpsleep.h"
#include "machine_rtc.h"

#include "sdkconfig.h"


// =========================================
// MicroPython runs as a task under FreeRTOS
// =========================================

#define MP_TASK_PRIORITY	CONFIG_MICROPY_TASK_PRIORITY
#define MP_TASK_STACK_SIZE	(CONFIG_MICROPY_STACK_SIZE * 1024)
#define MP_TASK_HEAP_SIZE	(CONFIG_MICROPY_HEAP_SIZE * 1024)
#define MP_TASK_STACK_LEN	(MP_TASK_STACK_SIZE / sizeof(StackType_t))


STATIC TaskHandle_t MainTaskHandle = NULL;
#if MICROPY_PY_THREAD
STATIC StaticTask_t DRAM_ATTR mp_task_tcb;
STATIC StackType_t DRAM_ATTR mp_task_stack[MP_TASK_STACK_LEN] __attribute__((aligned (8)));
#endif
STATIC uint8_t *mp_task_heap;

int MainTaskCore = 0;

//===============================
void mp_task(void *pvParameter) {
    volatile uint32_t sp = (uint32_t)get_sp();

    uart_init();

    mpsleep_init0();
    char rst_reason[24] = { 0 };
    mpsleep_get_reset_desc(rst_reason);
    printf("Reset reason: %s Wakeup: ", rst_reason);
    mpsleep_get_wake_desc(rst_reason);
    printf("%s\n", rst_reason);

    if (mpsleep_get_reset_cause() != MPSLEEP_DEEPSLEEP_RESET) {
        rtc_init0();
    }

	#if MICROPY_PY_THREAD
    mp_thread_preinit(&mp_task_stack[0], MP_TASK_STACK_LEN);
	#endif

    // Initialize the stack pointer for the main thread
    mp_stack_set_top((void *)sp);
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 1024);

soft_reset:
	// Thread init
	#if MICROPY_PY_THREAD
	mp_thread_init();
	#endif

    // initialize the mp heap
    gc_init(mp_task_heap, mp_task_heap + MP_TASK_HEAP_SIZE);

    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    mp_obj_list_init(mp_sys_argv, 0);
    readline_init0();

    // initialise peripherals
    machine_pins_init();

	// run boot-up scripts
    pyexec_frozen_module("_boot.py");
    pyexec_file("boot.py");
    if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
        pyexec_file("main.py");
    }

    // Main loop
    for (;;) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            if (pyexec_raw_repl() != 0) {
                break;
            }
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
    }

    #if MICROPY_PY_THREAD
    // delete all running threads
    mp_thread_deinit();
    #endif

    mp_hal_stdout_tx_str("ESP32: soft reboot\r\n");

    // deinitialise peripherals
    machine_pins_deinit();

    mp_deinit();
    fflush(stdout);
    goto soft_reset;
}
#include "libs/neopixel.h"
//============================
void micropython_entry(void) {
    nvs_flash_init();

    // === Set esp32 log level while running MicroPython ===
	esp_log_level_set("*", CONFIG_MICRO_PY_LOG_LEVEL);

    #if CONFIG_FREERTOS_UNICORE
    printf("\nFreeRTOS running only on FIRST CORE.\n");
    #else
    printf("\nFreeRTOS running on BOTH CORES, MicroPython task started on App Core.\n");
    #endif
    printf("\nuPY stack size = %d bytes\n", MP_TASK_STACK_LEN-1024);

    // ==== Allocate heap memory ====
    #if CONFIG_MEMMAP_SPIRAM_ENABLE
		// ## USING SPI RAM FOR HEAP ##
		#if !CONFIG_MEMMAP_SPIRAM_ENABLE_MALLOC
		printf("uPY  heap size = %d bytes (in SPIRAM using pvPortMallocCaps)\n\n", MP_TASK_HEAP_SIZE);
		mp_task_heap = pvPortMallocCaps(MP_TASK_HEAP_SIZE, MALLOC_CAP_SPIRAM);
		#else
		printf("uPY  heap size = %d bytes (in SPIRAM using malloc)\n\n", MP_TASK_HEAP_SIZE);
		mp_task_heap = malloc(MP_TASK_HEAP_SIZE);
		#endif
    #else
		// ## USING DRAM FOR HEAP ##
		printf("uPY  heap size = %d bytes\n\n", MP_TASK_HEAP_SIZE);
		mp_task_heap = malloc(MP_TASK_HEAP_SIZE);
    #endif

    if (mp_task_heap == NULL) {
        printf("Error allocating heap, Halted.\n");
        return;
    }

    // ==== Create and start main MicroPython task ====
    #if MICROPY_PY_THREAD
		// ==== THREADs ARE USED ====
		#if CONFIG_FREERTOS_UNICORE
			MainTaskCore = 0;
			MainTaskHandle = xTaskCreateStaticPinnedToCore(&mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &mp_task_stack[0], &mp_task_tcb, 0);
		#else
			MainTaskCore = 1;
			MainTaskHandle = xTaskCreateStaticPinnedToCore(&mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &mp_task_stack[0], &mp_task_tcb, 1);
		#endif
    #else
		// ==== THREADs ARE NOT USED ====
		#if CONFIG_FREERTOS_UNICORE
			MainTaskCore = 0;
			xTaskCreatePinnedToCore(&mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &MainTaskHandle, 0);
		#else
			MainTaskCore = 1;
			xTaskCreatePinnedToCore(&mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &MainTaskHandle, 1);
		#endif
    #endif
}

//-----------------------------
void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    esp_restart();
}

