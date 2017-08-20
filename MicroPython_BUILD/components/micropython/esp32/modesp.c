/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Paul Sokolovsky
 * Copyright (c) 2016 Damien P. George
 *
 * Copyright (c) 2017 Boris Lovosevic (wear leveling & sdmmc)
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

#include "sdkconfig.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "drivers/dht/dht.h"
#include "esprtcmem.h"


//---------------------------------------------------------------
STATIC mp_obj_t esp_rtcmem_write_(mp_obj_t _pos, mp_obj_t _val) {
	int pos = mp_obj_get_int(_pos);
	int val = mp_obj_get_int(_val);

	if (val < 0 || val > 255) {
		mp_raise_msg(&mp_type_IndexError, "Value out of range");
	}
	int res = esp_rtcmem_write(pos, val);
	if (res < 0) {
		mp_raise_msg(&mp_type_IndexError, "Offset out of range");
	}
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_rtcmem_write_obj, esp_rtcmem_write_);

//-----------------------------------------------
STATIC mp_obj_t esp_rtcmem_read_(mp_obj_t _pos) {
	int pos = mp_obj_get_int(_pos);

	int val = esp_rtcmem_read(pos);
	if (val < 0) {
		mp_raise_msg(&mp_type_IndexError, "Offset out of range");
	}
	return mp_obj_new_int(val);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_rtcmem_read_obj, esp_rtcmem_read_);

//-------------------------------------------------------------------------------
STATIC mp_obj_t esp_rtcmem_read_string_(mp_uint_t n_args, const mp_obj_t *args) {
	int pos = (n_args == 0) ? 2 : mp_obj_get_int(args[0]);

	char str[256];
	size_t str_len = sizeof(str);
	int res = esp_rtcmem_read_string(pos, str, &str_len);
	if (res < 0) {
		mp_raise_msg(&mp_type_IndexError, "Offset out of range");
	}
	return mp_obj_new_str(str, str_len-1, true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_rtcmem_read_string_obj, 0, 1, esp_rtcmem_read_string_);

//--------------------------------------------------------------------------------
STATIC mp_obj_t esp_rtcmem_write_string_(mp_uint_t n_args, const mp_obj_t *args) {
	const char *str = mp_obj_str_get_str(args[0]);
	int pos = (n_args == 1) ? 2 : mp_obj_get_int(args[1]);

	int res = esp_rtcmem_write_string(pos, str);
	if (res < 0) {
		mp_raise_msg(&mp_type_IndexError, "Offset out of range");
	}
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_rtcmem_write_string_obj, 1, 2, esp_rtcmem_write_string_);

// ====================================================================================================

//-----------------------------------------------------------
STATIC const mp_rom_map_elem_t esp_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_esp) },

    { MP_ROM_QSTR(MP_QSTR_dht_readinto), MP_ROM_PTR(&dht_readinto_obj) },

    {MP_ROM_QSTR(MP_QSTR_rtcmem_write), MP_ROM_PTR(&esp_rtcmem_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtcmem_read), MP_ROM_PTR(&esp_rtcmem_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtcmem_write_string), MP_ROM_PTR(&esp_rtcmem_write_string_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtcmem_read_string), MP_ROM_PTR(&esp_rtcmem_read_string_obj)},
};

STATIC MP_DEFINE_CONST_DICT(esp_module_globals, esp_module_globals_table);

//----------------------------------
const mp_obj_module_t esp_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&esp_module_globals,
};

