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

#include "py/runtime.h"
#include "py/stackctrl.h"

#if MICROPY_PY_THREAD

#include "py/mpthread.h"

#if 0 // print debugging info
#define DEBUG_PRINT (1)
#define DEBUG_printf DEBUG_printf
#else // don't print debugging info
#define DEBUG_PRINT (0)
#define DEBUG_printf(...) (void)0
#endif

extern TaskHandle_t MainTaskHandle;

/****************************************************************/
// Lock object

STATIC const mp_obj_type_t mp_type_thread_lock;

//------------------------------------
typedef struct _mp_obj_thread_lock_t {
    mp_obj_base_t base;
    mp_thread_mutex_t mutex;
    volatile bool locked;
} mp_obj_thread_lock_t;

//---------------------------------------------------------
STATIC mp_obj_thread_lock_t *mp_obj_new_thread_lock(void) {
    mp_obj_thread_lock_t *self = m_new_obj(mp_obj_thread_lock_t);
    self->base.type = &mp_type_thread_lock;
    mp_thread_mutex_init(&self->mutex);
    self->locked = false;
    return self;
}

//------------------------------------------------------------------------
STATIC mp_obj_t thread_lock_acquire(size_t n_args, const mp_obj_t *args) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(args[0]);
    bool wait = true;
    if (n_args > 1) {
        wait = mp_obj_get_int(args[1]);
        // TODO support timeout arg
    }
    MP_THREAD_GIL_EXIT();
    int ret = mp_thread_mutex_lock(&self->mutex, wait);
    MP_THREAD_GIL_ENTER();
    if (ret == 0) {
        return mp_const_false;
    } else if (ret == 1) {
        self->locked = true;
        return mp_const_true;
    } else {
        mp_raise_OSError(-ret);
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(thread_lock_acquire_obj, 1, 3, thread_lock_acquire);

//-----------------------------------------------------
STATIC mp_obj_t thread_lock_release(mp_obj_t self_in) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->locked) {
        mp_raise_msg(&mp_type_RuntimeError, NULL);
    }
    self->locked = false;
    MP_THREAD_GIL_EXIT();
    mp_thread_mutex_unlock(&self->mutex);
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(thread_lock_release_obj, thread_lock_release);

//----------------------------------------------------
STATIC mp_obj_t thread_lock_locked(mp_obj_t self_in) {
    mp_obj_thread_lock_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_bool(self->locked);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(thread_lock_locked_obj, thread_lock_locked);

//-------------------------------------------------------------------------
STATIC mp_obj_t thread_lock___exit__(size_t n_args, const mp_obj_t *args) {
    (void)n_args; // unused
    return thread_lock_release(args[0]);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(thread_lock___exit___obj, 4, 4, thread_lock___exit__);

//----------------------------------------------------------------
STATIC const mp_rom_map_elem_t thread_lock_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_acquire), MP_ROM_PTR(&thread_lock_acquire_obj) },
    { MP_ROM_QSTR(MP_QSTR_release), MP_ROM_PTR(&thread_lock_release_obj) },
    { MP_ROM_QSTR(MP_QSTR_locked), MP_ROM_PTR(&thread_lock_locked_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&thread_lock_acquire_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&thread_lock___exit___obj) },
};

STATIC MP_DEFINE_CONST_DICT(thread_lock_locals_dict, thread_lock_locals_dict_table);

//------------------------------------------------
STATIC const mp_obj_type_t mp_type_thread_lock = {
    { &mp_type_type },
    .name = MP_QSTR_lock,
    .locals_dict = (mp_obj_dict_t*)&thread_lock_locals_dict,
};

/****************************************************************/
// _thread module

STATIC size_t thread_stack_size = 0;

//------------------------------------------
STATIC mp_obj_t mod_thread_get_ident(void) {
    return mp_obj_new_int_from_uint((uintptr_t)mp_thread_get_state());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_get_ident_obj, mod_thread_get_ident);

//--------------------------------------------------------------------------
STATIC mp_obj_t mod_thread_stack_size(size_t n_args, const mp_obj_t *args) {
    mp_obj_t ret = mp_obj_new_int_from_uint(thread_stack_size);
    if (n_args == 0) {
        thread_stack_size = 0;
    } else {
    	size_t stack_size = mp_obj_get_int(args[0]);;
        if (stack_size == 0) {
        	stack_size = MP_THREAD_DEFAULT_STACK_SIZE; //use default stack size
        }
        else {
            if (stack_size < MP_THREAD_MIN_STACK_SIZE) stack_size = MP_THREAD_MIN_STACK_SIZE;
            else if (stack_size > MP_THREAD_MAX_STACK_SIZE) stack_size = MP_THREAD_MAX_STACK_SIZE;
        }
        thread_stack_size = stack_size;
    }
    return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_thread_stack_size_obj, 0, 1, mod_thread_stack_size);

//-----------------------------------
typedef struct _thread_entry_args_t {
    mp_obj_dict_t *dict_locals;
    mp_obj_dict_t *dict_globals;
    size_t stack_size;
    mp_obj_t fun;
    size_t n_args;
    size_t n_kw;
    mp_obj_t args[];
} thread_entry_args_t;

//--------------------------------------
STATIC void *thread_entry(void *args_in)
{
    // Execution begins here for a new thread.  We do not have the GIL.

    thread_entry_args_t *args = (thread_entry_args_t*)args_in;

    mp_state_thread_t ts;
    mp_thread_set_state(&ts);

    mp_stack_set_top(&ts + 1); // need to include ts in root-pointer scan
    mp_stack_set_limit(args->stack_size);

    // set locals and globals from the calling context
    mp_locals_set(args->dict_locals);
    mp_globals_set(args->dict_globals);

    MP_THREAD_GIL_ENTER();

    // signal that we are set up and running
    mp_thread_start();

    // TODO set more thread-specific state here:
    //  mp_pending_exception? (root pointer)
    //  cur_exception (root pointer)

    DEBUG_printf("[thread] start ts=%p args=%p stack=%p\n", &ts, &args, MP_STATE_THREAD(stack_top));

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_call_function_n_kw(args->fun, args->n_args, args->n_kw, args->args);
        DEBUG_printf("[thread] END ts=%p\n", &ts);
        nlr_pop();
    } else {
        // uncaught exception
        // check for SystemExit
    	DEBUG_printf("[thread] EXCEPTION ts=%p\n", &ts);
        mp_obj_base_t *exc = (mp_obj_base_t*)nlr.ret_val;
        if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
            // swallow exception silently
        } else {
            // print exception out
            mp_printf(&mp_plat_print, "Unhandled exception in thread started by ");
            mp_obj_print_helper(&mp_plat_print, args->fun, PRINT_REPR);
            mp_printf(&mp_plat_print, "\n");
            mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(exc));
        }
    }

    DEBUG_printf("[thread] finish ts=%p\n", &ts);

    MP_THREAD_GIL_EXIT();

    // signal that we are finished
    mp_thread_finish();

    return NULL;
}

//------------------------------------------------------------------------------
STATIC mp_obj_t mod_thread_start_new_thread(size_t n_args, const mp_obj_t *args)
{
    // This structure holds the Python function and arguments for thread entry.
    // We copy all arguments into this structure to keep ownership of them.
    // We must be very careful about root pointers because this pointer may
    // disappear from our address space before the thread is created.
    thread_entry_args_t *th_args;

    char *name = NULL;
	if (MP_OBJ_IS_STR(args[0])) {
		name = mp_obj_str_get_str(args[0]);
	}
	else {
        mp_raise_TypeError("expecting a string for thread name argument");
	}
    // get positional arguments
    size_t pos_args_len;
    mp_obj_t *pos_args_items;
    mp_obj_get_array(args[2], &pos_args_len, &pos_args_items);

    // check for keyword arguments
    if (n_args == 3) {
        // just position arguments
        th_args = m_new_obj_var(thread_entry_args_t, mp_obj_t, pos_args_len);
        th_args->n_kw = 0;
    } else {
        // positional and keyword arguments
        if (mp_obj_get_type(args[3]) != &mp_type_dict) {
            mp_raise_TypeError("expecting a dict for keyword args");
        }
        mp_map_t *map = &((mp_obj_dict_t*)MP_OBJ_TO_PTR(args[3]))->map;
        th_args = m_new_obj_var(thread_entry_args_t, mp_obj_t, pos_args_len + 2 * map->used);
        th_args->n_kw = map->used;
        // copy across the keyword arguments
        for (size_t i = 0, n = pos_args_len; i < map->alloc; ++i) {
            if (MP_MAP_SLOT_IS_FILLED(map, i)) {
                th_args->args[n++] = map->table[i].key;
                th_args->args[n++] = map->table[i].value;
            }
        }
    }

    // copy agross the positional arguments
    th_args->n_args = pos_args_len;
    memcpy(th_args->args, pos_args_items, pos_args_len * sizeof(mp_obj_t));

    // pass our locals and globals into the new thread
    th_args->dict_locals = mp_locals_get();
    th_args->dict_globals = mp_globals_get();

    // set the stack size to use
    th_args->stack_size = thread_stack_size;

    // set the function for thread entry
    th_args->fun = args[1];

    // spawn the thread!
    uintptr_t thr_id = (uintptr_t)mp_thread_create(thread_entry, th_args, &th_args->stack_size, name);

    return mp_obj_new_int_from_uint((uintptr_t)thr_id);
    //return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_thread_start_new_thread_obj, 3, 4, mod_thread_start_new_thread);

//-------------------------------------
STATIC mp_obj_t mod_thread_exit(void) {
    nlr_raise(mp_obj_new_exception(&mp_type_SystemExit));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_exit_obj, mod_thread_exit);

//----------------------------------------------
STATIC mp_obj_t mod_thread_allocate_lock(void) {
    return MP_OBJ_FROM_PTR(mp_obj_new_thread_lock());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_allocate_lock_obj, mod_thread_allocate_lock);


/****************************************************************/
// Inter - thread notifications and messaging


//-------------------------------------------------------
STATIC mp_obj_t mod_thread_allowsuspend(mp_obj_t in_id) {
	int allow = mp_obj_is_true(in_id);

	mp_thread_allowsuspend(allow);
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_thread_allowsuspend_obj, mod_thread_allowsuspend);

//--------------------------------------------------
STATIC mp_obj_t mod_thread_suspend(mp_obj_t in_id) {
	uintptr_t thr_id = mp_obj_get_int(in_id);

	if (mp_thread_suspend((void *)thr_id)) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_thread_suspend_obj, mod_thread_suspend);

//-------------------------------------------------
STATIC mp_obj_t mod_thread_resume(mp_obj_t in_id) {
	uintptr_t thr_id = mp_obj_get_int(in_id);

	if (mp_thread_resume((void *)thr_id)) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_thread_resume_obj, mod_thread_resume);

//-----------------------------------------------
STATIC mp_obj_t mod_thread_stop(mp_obj_t in_id) {
	uintptr_t thr_id = mp_obj_get_int(in_id);

	if (mp_thread_stop((void *)thr_id)) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_thread_stop_obj, mod_thread_stop);

//--------------------------------------------------------------------
STATIC mp_obj_t mod_thread_notify(mp_obj_t in_id, mp_obj_t in_value) {
	uintptr_t thr_id = mp_obj_get_int(in_id);
	uint32_t notify_value = mp_obj_get_int(in_value);
	if (notify_value == 0) return mp_const_false;

	if (mp_thread_notify((void *)thr_id, notify_value)) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_thread_notify_obj, mod_thread_notify);

//--------------------------------------
STATIC mp_obj_t mod_thread_getREPLId() {
    return mp_obj_new_int_from_uint((uintptr_t)MainTaskHandle);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_getREPLId_obj, mod_thread_getREPLId);

//--------------------------------------
STATIC mp_obj_t mod_thread_getnotify() {

	uint32_t not_val = mp_thread_getnotify();
    return MP_OBJ_NEW_SMALL_INT(not_val);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_getnotify_obj, mod_thread_getnotify);

//----------------------------------------------------------------------------------------------
STATIC mp_obj_t mod_thread_sendmsg(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    const mp_arg_t allowed_args[] = {
       { MP_QSTR_ThreadID, MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
       { MP_QSTR_Message,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    const char *msg = NULL;
    size_t msglen = 0;
    mp_int_t msg_int = 0;
    uintptr_t thr_id = 1;

    int type = THREAD_MSG_TYPE_INTEGER;

	if (MP_OBJ_IS_INT(args[0].u_obj)) {
		thr_id = mp_obj_get_int((mp_const_obj_t)args[0].u_obj);
	}
	else return mp_const_false;

	if (MP_OBJ_IS_STR(args[1].u_obj)) {
        msg = mp_obj_str_get_data(args[1].u_obj, &msglen);
        if (msglen == 0) return mp_const_false;
        type = THREAD_MSG_TYPE_STRING;
    }
	else if (MP_OBJ_IS_INT(args[1].u_obj)) {
    	msg_int = mp_obj_get_int(args[1].u_obj);
    }
	else return mp_const_false;

	int res = mp_thread_semdmsg((void *)thr_id, type, msg_int, (uint8_t *)msg, msglen);

	return MP_OBJ_NEW_SMALL_INT(res);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_thread_sendmsg_obj, 2, mod_thread_sendmsg);

//---------------------------------
STATIC mp_obj_t mod_thread_getmsg()
{
	uint8_t *buf = NULL;
	int msg_int = 0;
	int res = 0;
	int buflen = 0;
	uintptr_t sender = 0;
    mp_obj_t tuple[3];

	res = mp_thread_getmsg(&msg_int, &buf, &buflen, (uint32_t *)&sender);

	tuple[0] = mp_obj_new_int(res);
    tuple[1] = mp_obj_new_int(sender);
	if (res == THREAD_MSG_TYPE_NONE) {
		tuple[2] = mp_const_none;
	}
	else if (res == THREAD_MSG_TYPE_INTEGER) {
	    tuple[2] = mp_obj_new_int(msg_int);
	}
	else if (res == THREAD_MSG_TYPE_STRING) {
		if (buf != NULL) {
			tuple[2] = mp_obj_new_str((char *)buf, buflen, false);
			free(buf);
		}
		else tuple[2] = mp_const_none;
	}

    return mp_obj_new_tuple(3, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_getmsg_obj, mod_thread_getmsg);

//--------------------------------------------------
STATIC mp_obj_t mod_thread_getname(mp_obj_t in_id) {
	uintptr_t thr_id = mp_obj_get_int(in_id);
	char name[THREAD_NAME_MAX_SIZE] = {'\0'};

	int res = mp_thread_getname((void *)thr_id, name);
	if (!res) {
		sprintf(name,"unknown");
	}
	return mp_obj_new_str((char *)name, strlen(name), false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_thread_getname_obj, mod_thread_getname);

//------------------------------------------------------
STATIC mp_obj_t mod_thread_getSelfname() {
	char name[THREAD_NAME_MAX_SIZE] = {'\0'};

	int res = mp_thread_getSelfname(name);
	if (!res) {
		sprintf(name,"unknown");
	}
	return mp_obj_new_str((char *)name, strlen(name), false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_getSelfname_obj, mod_thread_getSelfname);

//-----------------------------------------------------------------------
STATIC mp_obj_t mod_thread_list(mp_uint_t n_args, const mp_obj_t *args) {
	int prn = 1;
    if (n_args > 0) prn = mp_obj_is_true(args[0]);

	thread_list_t list = {0, NULL};

	uint32_t num = mp_thread_list(&list, prn);

	if ((num == 0) || (prn) || list.threads == NULL) return mp_const_none;

	mp_obj_t thr_info[3];
	mp_obj_t tuple[num];
	int n = 0;
	threadlistitem_t *thr = NULL;
	for (n=0; n<num; n++) {
		thr = list.threads + (sizeof(threadlistitem_t) * n);
		thr_info[0] = mp_obj_new_int(thr->id);
		thr_info[1] = mp_obj_new_str(thr->name, strlen(thr->name), false);
		if (thr->suspended) thr_info[2] = mp_const_true;
		else thr_info[2] = mp_const_false;
		tuple[n] = mp_obj_new_tuple(3, thr_info);
	}
	free(list.threads);

    return mp_obj_new_tuple(n, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_thread_list_obj, 0, 1, mod_thread_list);

//-----------------------------------------------------------------------
STATIC mp_obj_t mod_thread_replAcceptMsg(mp_uint_t n_args, const mp_obj_t *args) {
	int res = 0;
    if (n_args == 0) {
    	res = mp_thread_replAcceptMsg(-1);
    }
    else {
    	res = mp_thread_replAcceptMsg(mp_obj_is_true(args[0]));
    }
	return mp_obj_new_bool(res);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_thread_replAcceptMsg_obj, 0, 1, mod_thread_replAcceptMsg);


//=================================================================
STATIC const mp_rom_map_elem_t mp_module_thread_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__thread) },
    { MP_ROM_QSTR(MP_QSTR_LockType), MP_ROM_PTR(&mp_type_thread_lock) },
    { MP_ROM_QSTR(MP_QSTR_get_ident), MP_ROM_PTR(&mod_thread_get_ident_obj) },
    { MP_ROM_QSTR(MP_QSTR_stack_size), MP_ROM_PTR(&mod_thread_stack_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_start_new_thread), MP_ROM_PTR(&mod_thread_start_new_thread_obj) },
    { MP_ROM_QSTR(MP_QSTR_exit), MP_ROM_PTR(&mod_thread_exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_allocate_lock), MP_ROM_PTR(&mod_thread_allocate_lock_obj) },
    { MP_ROM_QSTR(MP_QSTR_allowsuspend), MP_ROM_PTR(&mod_thread_allowsuspend_obj) },
    { MP_ROM_QSTR(MP_QSTR_suspend), MP_ROM_PTR(&mod_thread_suspend_obj) },
    { MP_ROM_QSTR(MP_QSTR_resume), MP_ROM_PTR(&mod_thread_resume_obj) },
    { MP_ROM_QSTR(MP_QSTR_kill), MP_ROM_PTR(&mod_thread_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_notify), MP_ROM_PTR(&mod_thread_notify_obj) },
    { MP_ROM_QSTR(MP_QSTR_getReplID), MP_ROM_PTR(&mod_thread_getREPLId_obj) },
    { MP_ROM_QSTR(MP_QSTR_replAcceptMsg), MP_ROM_PTR(&mod_thread_replAcceptMsg_obj) },
    { MP_ROM_QSTR(MP_QSTR_getnotification), MP_ROM_PTR(&mod_thread_getnotify_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendmsg), MP_ROM_PTR(&mod_thread_sendmsg_obj) },
    { MP_ROM_QSTR(MP_QSTR_getmsg), MP_ROM_PTR(&mod_thread_getmsg_obj) },
    { MP_ROM_QSTR(MP_QSTR_list), MP_ROM_PTR(&mod_thread_list_obj) },
    { MP_ROM_QSTR(MP_QSTR_getThreadName), MP_ROM_PTR(&mod_thread_getname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getSelfName), MP_ROM_PTR(&mod_thread_getSelfname_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_thread_globals, mp_module_thread_globals_table);

//========================================
const mp_obj_module_t mp_module_thread = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_thread_globals,
};

#endif // MICROPY_PY_THREAD
