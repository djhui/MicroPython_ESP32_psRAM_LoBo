/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George on behalf of Pycom Ltd
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

#ifndef __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
#define __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__

#if MICROPY_PY_THREAD

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

//ToDo: Check if thread can run on different priority than main task
//#if CONFIG_MICROPY_THREAD_PRIORITY > CONFIG_MICROPY_TASK_PRIORITY
//#define MP_THREAD_PRIORITY	CONFIG_MICROPY_THREAD_PRIORITY
//#else
#define MP_THREAD_PRIORITY	CONFIG_MICROPY_TASK_PRIORITY
//#endif

#define MP_THREAD_MIN_STACK_SIZE		CONFIG_MICROPY_THREAD_STACK_SIZE*1024
#define MP_THREAD_DEFAULT_STACK_SIZE	(MP_THREAD_MIN_STACK_SIZE)
#define MP_THREAD_MAX_STACK_SIZE		32*1024

typedef struct _mp_thread_mutex_t {
    #ifndef BOOTLOADER
    SemaphoreHandle_t handle;
    StaticSemaphore_t buffer;
    #endif
} mp_thread_mutex_t;

void mp_thread_preinit(void *stack, uint32_t stack_len);
void mp_thread_init(void);
void mp_thread_gc_others(void);
void mp_thread_deinit(void);

int mp_thread_suspend(TaskHandle_t id);
int mp_thread_resume(TaskHandle_t id);
int mp_thread_stop(TaskHandle_t id);
int mp_thread_notify(TaskHandle_t id, uint32_t value);
uint32_t mp_thread_getnotify();

#endif

#endif // __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
