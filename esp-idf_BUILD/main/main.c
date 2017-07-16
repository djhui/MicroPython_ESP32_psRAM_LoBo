/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
//#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task.h"
#include "soc/cpu.h"
#include "esp_log.h"

#include "sdkconfig.h"

#include "py/stackctrl.h"
//#include "py/nlr.h"
//#include "py/compile.h"
#include "py/runtime.h"
//#include "py/repl.h"
#include "py/gc.h"
//#include "py/mphal.h"
#include "lib/mp-readline/readline.h"
#include "lib/utils/pyexec.h"
#include "uart.h"
//#include "modmachine.h"
#include "mpthreadport.h"
#include "mpsleep.h"
#include "machrtc.h"





#define MP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#if CONFIG_MEMMAP_SPIRAM_ENABLE
// External SPIRAM is available, more memory for stack & heap can be allocated
#define MP_TASK_STACK_SIZE      (64 * 1024)
#define MP_TASK_HEAP_SIZE       (4 * 1024 * 1024 - 256)
#else
// Only DRAM memory available, limited amount of memory for stack & heap can be used
#define MP_TASK_STACK_SIZE      (16 * 1024)
#define MP_TASK_HEAP_SIZE       (92 * 1024)
#endif
#define MP_TASK_STACK_LEN       (MP_TASK_STACK_SIZE / sizeof(StackType_t))


static StaticTask_t mp_task_tcb;
//static StackType_t mp_task_stack[MP_TASK_STACK_LEN] __attribute__((aligned (8)));
static StackType_t *mp_task_stack;
static uint8_t *mp_task_heap;


//===============================
void mp_task(void *pvParameter) {
    volatile uint32_t sp = (uint32_t)get_sp();

    mp_thread_init(mp_task_stack, MP_TASK_STACK_LEN);

    uart_init();

    mpsleep_init0();

    printf("Reset cause: %d\n", mpsleep_get_reset_cause());
 
    // initialise the stack pointer for the main thread
    mp_stack_set_top((void *)sp);
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 1024);

    // initialize the mp heap
    gc_init(mp_task_heap, mp_task_heap + MP_TASK_HEAP_SIZE);

    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    mp_obj_list_init(mp_sys_argv, 0);
    readline_init0();

    // initialise peripherals
    //machine_pins_init();

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

    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        printf("OK.\n");
    }
}


//=============
void app_main()
{
    nvs_flash_init();

	esp_log_level_set("*", ESP_LOG_ERROR);

    printf("\nAllocating uPY stack, size=%d\n", MP_TASK_STACK_LEN);
    mp_task_stack = (StackType_t *)pvPortMallocCaps(MP_TASK_STACK_LEN, MALLOC_CAP_8BIT);
    if (mp_task_stack == NULL) {
        printf("Error, Halted\n");
        return;
    }

    // Allocate heap memory
    #if !CONFIG_MEMMAP_SPIRAM_ENABLE_MALLOC
    printf("\nAllocating uPY heap (%d bytes) in SPIRAM using pvPortMallocCaps\n\n", MP_TASK_HEAP_SIZE);
    mp_task_heap = pvPortMallocCaps(MP_TASK_HEAP_SIZE, MALLOC_CAP_SPIRAM);
    #else
    #if CONFIG_MEMMAP_SPIRAM_ENABLE
    printf("\nAllocating uPY heap (%d bytes) in SPIRAM using malloc\n\n", MP_TASK_HEAP_SIZE);
    #else
    printf("\nAllocating uPY heap (%d bytes) in DRAM using malloc\n\n", MP_TASK_HEAP_SIZE);
    #endif
    mp_task_heap = malloc(MP_TASK_HEAP_SIZE);
    #endif
    if (mp_task_heap == NULL) {
        printf("Error, Halted\n");
        return;
    }

    //xTaskCreateStaticPinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &mp_task_stack[0], &mp_task_tcb, 0);
    xTaskCreateStaticPinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, mp_task_stack, &mp_task_tcb, 1);
}

//-----------------------------
void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    esp_restart();
}

// modussl_mbedtls uses this function but it's not enabled in ESP IDF
//-----------------------------------------------
void mbedtls_debug_set_threshold(int threshold) {
    (void)threshold;
}
