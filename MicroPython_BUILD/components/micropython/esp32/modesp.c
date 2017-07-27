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
#include "modesp.h"


//--------------------------------------------------------------------------------
STATIC mp_obj_t esp_neopixel_write_(mp_obj_t pin, mp_obj_t buf, mp_obj_t timing) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);
    esp_neopixel_write(mp_hal_get_pin_obj(pin),
        (uint8_t*)bufinfo.buf, bufinfo.len, mp_obj_get_int(timing));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(esp_neopixel_write_obj, esp_neopixel_write_);


// ====================================================================================================

//-----------------------------------------------------------
STATIC const mp_rom_map_elem_t esp_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_esp) },

    { MP_ROM_QSTR(MP_QSTR_neopixel_write), MP_ROM_PTR(&esp_neopixel_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_dht_readinto), MP_ROM_PTR(&dht_readinto_obj) },

};

STATIC MP_DEFINE_CONST_DICT(esp_module_globals, esp_module_globals_table);

//----------------------------------
const mp_obj_module_t esp_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&esp_module_globals,
};

