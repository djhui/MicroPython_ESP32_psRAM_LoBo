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

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "py/mpconfig.h"

#if MICROPY_PY_THREAD

#include "esp_attr.h"

#include "py/mpstate.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "mpthreadport.h"

#include "sdkconfig.h"


extern int MainTaskCore;

TaskHandle_t MainTaskHandle = NULL;

uint8_t main_accept_msg = 1;

// this structure forms a linked list, one node per active thread
//========================
typedef struct _thread_t {
    TaskHandle_t id;					// system id of thread
    int ready;							// whether the thread is ready and running
    void *arg;							// thread Python args, a GC root pointer
    void *stack;						// pointer to the stack
    StaticTask_t *tcb;     				// pointer to the Task Control Block
    size_t stack_len;      				// number of words in the stack
    char name[THREAD_NAME_MAX_SIZE];	// thread name
    QueueHandle_t threadQueue;			// queue used for inter thread communication
    int allow_suspend;
    int suspended;

    struct _thread_t *next;
} thread_t;

// the mutex controls access to the linked list
STATIC mp_thread_mutex_t thread_mutex;
STATIC thread_t thread_entry0;
STATIC thread_t *thread; // root pointer, handled by mp_thread_gc_others

// === Initialize the main MicroPython thread ===
//-------------------------------------------------------
void mp_thread_preinit(void *stack, uint32_t stack_len) {
    mp_thread_set_state(&mp_state_ctx.thread);
    // create first entry in linked list of all threads
    thread = &thread_entry0;
    thread->id = xTaskGetCurrentTaskHandle();
    thread->ready = 1;
    thread->arg = NULL;
    thread->stack = stack;
    thread->stack_len = stack_len;
    sprintf(thread->name, "MainThread");
    thread->threadQueue = xQueueCreate( THREAD_QUEUE_MAX_ITEMS, sizeof(thread_msg_t) );
    thread->allow_suspend = 0;
    thread->suspended = 0;
    thread->next = NULL;
    MainTaskHandle = thread->id;
}

//-------------------------
void mp_thread_init(void) {
    mp_thread_mutex_init(&thread_mutex);
}

//------------------------------
void mp_thread_gc_others(void) {
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        gc_collect_root((void**)&th, 1);
        gc_collect_root(&th->arg, 1); // probably not needed
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if (!th->ready) {
            continue;
        }
        //ToDo: Check if needed
        gc_collect_root(th->stack, th->stack_len); // probably not needed
    }
    mp_thread_mutex_unlock(&thread_mutex);
}

//--------------------------------------------
mp_state_thread_t *mp_thread_get_state(void) {
    return pvTaskGetThreadLocalStoragePointer(NULL, 1);
}

//-------------------------------------
void mp_thread_set_state(void *state) {
    vTaskSetThreadLocalStoragePointer(NULL, 1, state);
}

//--------------------------
void mp_thread_start(void) {
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
            th->ready = 1;
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
}

STATIC void *(*ext_thread_entry)(void*) = NULL;

//-------------------------------------
STATIC void freertos_entry(void *arg) {
    if (ext_thread_entry) {
        ext_thread_entry(arg);
    }
    vTaskDelete(NULL);
    //for (;;);
}

//--------------------------------------------------------------------------------------------------------------
TaskHandle_t mp_thread_create_ex(void *(*entry)(void*), void *arg, size_t *stack_size, int priority, char *name)
{
    // store thread entry function into a global variable so we can access it
    ext_thread_entry = entry;

    // Check thread stack size
    if (*stack_size == 0) {
    	*stack_size = MP_THREAD_DEFAULT_STACK_SIZE; //use default stack size
    }
    else {
        if (*stack_size < MP_THREAD_MIN_STACK_SIZE) *stack_size = MP_THREAD_MIN_STACK_SIZE;
        else if (*stack_size > MP_THREAD_MAX_STACK_SIZE) *stack_size = MP_THREAD_MAX_STACK_SIZE;
    }

    // allocate TCB, stack and linked-list node (must be outside thread_mutex lock)
    //StaticTask_t *tcb = m_new(StaticTask_t, 1);
    //StackType_t *stack = m_new(StackType_t, *stack_size / sizeof(StackType_t));

    // ======================================================================
    // We are NOT going to allocate thread tcb & stack on Micropython heap
    // In case we are using SPI RAM, it can produce some problems and crashes
    // ======================================================================
    StaticTask_t *tcb = NULL;
    StackType_t *stack = NULL;

    tcb = malloc(sizeof(StaticTask_t));
    stack = malloc(*stack_size);

    thread_t *th = m_new_obj(thread_t);

    mp_thread_mutex_lock(&thread_mutex, 1);

    // create thread task pinned to the same core as the main task
    //ToDo: Check if it can run on different core
    TaskHandle_t id = xTaskCreateStaticPinnedToCore(freertos_entry, name, *stack_size / sizeof(StackType_t), arg, priority, stack, tcb, MainTaskCore);
    if (id == NULL) {
        mp_thread_mutex_unlock(&thread_mutex);
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "can't create thread"));
    }

    // adjust the stack_size to provide room to recover from hitting the limit
    *stack_size -= 1024;

    // add thread to linked list of all threads
    th->id = id;
    th->ready = 0;
    th->arg = arg;
    th->stack = stack;
    th->tcb = tcb;
    th->stack_len = *stack_size / sizeof(StackType_t);
    th->next = thread;
    snprintf(th->name, THREAD_NAME_MAX_SIZE, name);
    th->threadQueue = xQueueCreate( THREAD_QUEUE_MAX_ITEMS, sizeof(thread_msg_t) );
    th->allow_suspend = 0;
    th->suspended = 0;
    thread = th;

    mp_thread_mutex_unlock(&thread_mutex);
    return id;
}

//----------------------------------------------------------------------------------------
void *mp_thread_create(void *(*entry)(void*), void *arg, size_t *stack_size, char *name) {
    return mp_thread_create_ex(entry, arg, stack_size, MP_THREAD_PRIORITY, name);
}

//---------------------------------------
STATIC void mp_clean_thread(thread_t *th)
{
    int n = 1;
    while (n) {
    	n = uxQueueMessagesWaiting(th->threadQueue);
    	if (n) {
    		thread_msg_t msg;
    		xQueueReceive(th->threadQueue, &msg, 0);
    		if (msg.strdata != NULL) free(msg.strdata);
    	}
    }
    if (th->threadQueue) vQueueDelete(th->threadQueue);
    th->threadQueue = NULL;
    th->ready = 0;
    if (th->tcb) free(th->tcb);
    if (th->stack) free(th->stack);
}
//---------------------------
void mp_thread_finish(void) {
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
        	mp_clean_thread(th);
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
}

//--------------------------------
void vPortCleanUpTCB (void *tcb) {
    thread_t *prev = NULL;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; prev = th, th = th->next) {
        // unlink the node from the list
        if (th->tcb == tcb) {
            if (prev != NULL) {
                prev->next = th->next;
            } else {
                // move the start pointer
                thread = th->next;
            }
            // Explicitly release all its memory
            //m_del(StaticTask_t, th->mtcb, 1);
            //m_del(StackType_t, th->mstack, th->stack_len);
            m_del(thread_t, th, 1);
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
}

//---------------------------------------------------
void mp_thread_mutex_init(mp_thread_mutex_t *mutex) {
    mutex->handle = xSemaphoreCreateMutexStatic(&mutex->buffer);
}

//------------------------------------------------------------
int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait) {
    return (pdTRUE == xSemaphoreTake(mutex->handle, wait ? portMAX_DELAY : 0));
}

//-----------------------------------------------------
void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex) {
    xSemaphoreGive(mutex->handle);
}

//---------------------------
void mp_thread_deinit(void) {
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't delete the current task
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
    	mp_clean_thread(th);
        vTaskDelete(th->id);
    }
    mp_thread_mutex_unlock(&thread_mutex);
    // allow FreeRTOS to clean-up the threads
    vTaskDelay(2);
}

//--------------------------------------
void mp_thread_allowsuspend(int allow) {
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't allow suspending main task task
        if ((th->id != MainTaskHandle) && (th->id == xTaskGetCurrentTaskHandle())) {
        	th->allow_suspend = allow & 1;
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
}

//--------------------------------------
int mp_thread_suspend(TaskHandle_t id) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't suspend the current task
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if (th->id == id) {
        	if ((th->allow_suspend) && (th->suspended == 0)) {
        		th->suspended = 1;
        		vTaskSuspend(th->id);
        		res = 1;
        	}
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return res;
}

//-------------------------------------
int mp_thread_resume(TaskHandle_t id) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't resume the current task
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if (th->id == id) {
        	if ((th->allow_suspend) && (th->suspended)) {
        		th->suspended = 0;
        		vTaskResume(th->id);
        		res = 1;
        	}
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return res;
}

//-----------------------------------
int mp_thread_stop(TaskHandle_t id) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't stop the current task
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if (th->id == id) {
        	mp_clean_thread(th);
            vTaskDelete(th->id);
            res = 1;
            break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    // allow FreeRTOS to clean-up the threads
    vTaskDelay(2);
    return res;
}

//-----------------------------------------------------
int mp_thread_notify(TaskHandle_t id, uint32_t value) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't notify the sending task
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if ((id == 0) || (th->id == id)) {
        	xTaskNotify(th->id, value, eSetBits);
            res = 1;
            if (id != 0) break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return res;
}

//------------------------------
uint32_t mp_thread_getnotify() {
	uint32_t value = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
        	if (xTaskNotifyWait(0, 0xffffffffUL, &value, 1) != pdPASS) {
        		value = 0;
        	}
			break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return value;
}

//------------------------------
uint32_t mp_thread_getSelfID() {
	uint32_t id = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
        	id = (uint32_t)th->id;
			break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return id;
}

//-------------------------------------
int mp_thread_getSelfname(char *name) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == xTaskGetCurrentTaskHandle()) {
        	sprintf(name, th->name);
        	res = 1;
			break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return res;
}

//--------------------------------------------------
int mp_thread_getname(TaskHandle_t id, char *name) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == id) {
        	sprintf(name, th->name);
        	res = 1;
			break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return res;
}

//---------------------------------------------------------------------------------------
int mp_thread_semdmsg(TaskHandle_t id, int type, int msg_int, uint8_t *buf, int buflen) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // don't send to the current task
        if (th->id == xTaskGetCurrentTaskHandle()) {
            continue;
        }
        if ((id == 0) || (th->id == id)) {
        	if (th->threadQueue == NULL) break;
    		thread_msg_t msg;
    	    struct timeval tv;
    	    uint64_t tmstamp;
    	    gettimeofday(&tv, NULL);
    	    tmstamp = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    	    msg.timestamp = tmstamp;
			msg.sender_id = xTaskGetCurrentTaskHandle();
			if (type == THREAD_MSG_TYPE_INTEGER) {
				msg.intdata = msg_int;
				msg.strdata = NULL;
				msg.type = type;
				res = 1;
			}
			else if (type == THREAD_MSG_TYPE_STRING) {
				msg.intdata = buflen;
				msg.strdata = malloc(buflen+1);
				if (msg.strdata != NULL) {
					memcpy(msg.strdata, buf, buflen);
					msg.strdata[buflen] = 0;
					msg.type = type;
					res = 1;
				}
			}
			if (res) {
				if (xQueueSend(th->threadQueue, &msg, 0) != pdTRUE) res = 0;
			}
            if (id != 0) break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return res;
}

//--------------------------------------------------------------------------------
int mp_thread_getmsg(int *msg_int, uint8_t **buf, int *buflen, uint32_t *sender) {
	int res = 0;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
        // get message for current task
        if (th->id == xTaskGetCurrentTaskHandle()) {
        	if (th->threadQueue == NULL) break;

        	thread_msg_t msg;
        	if (xQueueReceive(th->threadQueue, &msg, 0) == pdTRUE) {
        		*sender = (uint32_t)msg.sender_id;
        		if (msg.type == THREAD_MSG_TYPE_INTEGER) {
        			*msg_int = msg.intdata;
        			*buflen = 0;
        			res = THREAD_MSG_TYPE_INTEGER;
        		}
        		else if (msg.type == THREAD_MSG_TYPE_STRING) {
        			*msg_int = 0;
        			if ((msg.strdata != NULL) && (msg.intdata > 0)) {
            			*buflen = msg.intdata;
            			*buf = msg.strdata;
            			res = THREAD_MSG_TYPE_STRING;
        			}
        		}
        	}
        	break;
        }
    }
    mp_thread_mutex_unlock(&thread_mutex);

    return res;
}

//------------------------------------------------
int mp_thread_list(thread_list_t *list, int prn) {
	int num = 0;

    mp_thread_mutex_lock(&thread_mutex, 1);

    for (thread_t *th = thread; th != NULL; th = th->next) {
    	num++;
		if (prn) printf("ID=%u, Name: %s, State: %s, Stack=%d\n",
				(uint32_t)th->id, th->name, (th->suspended ? "suspended" : "running"), th->stack_len);
    }
    if ((num == 0) || (prn) || (list == NULL)) {
        mp_thread_mutex_unlock(&thread_mutex);
    	return num;
    }

	list->nth = num;
	list->threads = malloc(sizeof(threadlistitem_t) * num);
	if (list->threads == NULL) num = 0;
	else {
		int nth = 0;
		threadlistitem_t *thr = NULL;
		for (thread_t *th = thread; th != NULL; th = th->next) {
			thr = list->threads + (sizeof(threadlistitem_t) * nth);
			thr->id = (uint32_t)th->id;
			sprintf(thr->name, "%s", th->name);
			thr->suspended = th->suspended;
			nth++;
		}
		if (nth != num) {
			free(list->threads);
			list->threads = NULL;
			num = 0;
		}
    }
    mp_thread_mutex_unlock(&thread_mutex);
    return num;
}

//------------------------------------------
int mp_thread_replAcceptMsg(int8_t accept) {
	int res = main_accept_msg;
    mp_thread_mutex_lock(&thread_mutex, 1);
    for (thread_t *th = thread; th != NULL; th = th->next) {
    	 if (th->id == xTaskGetCurrentTaskHandle()) {
    		 if ((th->id == MainTaskHandle) && (accept >= 0)) {
    			 main_accept_msg = accept & 1;
    		 }
			 break;
    	 }
    }
    mp_thread_mutex_unlock(&thread_mutex);

    return res;
}


#else

void vPortCleanUpTCB (void *tcb) {

}

#endif // MICROPY_PY_THREAD
