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

#define MP_THREAD_MIN_STACK_SIZE		(2*1024)
#define MP_THREAD_DEFAULT_STACK_SIZE	(CONFIG_MICROPY_THREAD_STACK_SIZE*1024)
#define MP_THREAD_MAX_STACK_SIZE		(16*1024)

typedef struct _mp_thread_mutex_t {
    SemaphoreHandle_t handle;
    StaticSemaphore_t buffer;
} mp_thread_mutex_t;

#define THREAD_NAME_MAX_SIZE		17
#define THREAD_MGG_BROADCAST		0xFFFFEEEE
#define THREAD_MSG_TYPE_NONE		0
#define THREAD_MSG_TYPE_INTEGER		1
#define THREAD_MSG_TYPE_STRING		2
#define MAX_THREAD_MESSAGES			8
#define THREAD_QUEUE_MAX_ITEMS		8

// this structure is used for inter-thread communication/data passing
typedef struct _thread_msg_t {
    int type;						// message type
    TaskHandle_t sender_id;			// id of the message sender
    int intdata;					// integer data or string data length
    uint8_t *strdata;				// string data
    uint32_t timestamp;				// message timestamp in ms
} thread_msg_t;

typedef struct _thread_listitem_t {
    uint32_t id;						// thread id
    char name[THREAD_NAME_MAX_SIZE];	// thread name
    int suspended;
} threadlistitem_t;

typedef struct _thread_list_t {
    int nth;						// number of active threads
    threadlistitem_t *threads;		// pointer to thread info
} thread_list_t;

thread_msg_t thread_messages[MAX_THREAD_MESSAGES];

uint8_t main_accept_msg;

void mp_thread_preinit(void *stack, uint32_t stack_len);
void mp_thread_init(void);
void mp_thread_gc_others(void);
void mp_thread_deinit(void);

void mp_thread_allowsuspend(int allow);
int mp_thread_suspend(TaskHandle_t id);
int mp_thread_resume(TaskHandle_t id);
int mp_thread_stop(TaskHandle_t id);
int mp_thread_notify(TaskHandle_t id, uint32_t value);
uint32_t mp_thread_getnotify();
int mp_thread_semdmsg(TaskHandle_t id, int type, int msg_int, uint8_t *buf, int buflen);
int mp_thread_getmsg(int *msg_int, uint8_t **buf, int *buflen, uint32_t *sender);

uint32_t mp_thread_getSelfID();
int mp_thread_getSelfname(char *name);
int mp_thread_getname(TaskHandle_t id, char *name);
int mp_thread_list(thread_list_t *list, int prn);
int mp_thread_replAcceptMsg(int8_t accept);

#endif

#endif // __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
