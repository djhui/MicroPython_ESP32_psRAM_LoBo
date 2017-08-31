/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 "Eric Poulsen" <eric@zyxod.com>
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
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#ifdef IDF_USEHEAP
#include "esp_heap_caps.h"
#else
#include "esp_heap_alloc_caps.h"
#endif

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "modmachine.h"
#include "machine_pin.h"

#include "tft/spi_master_lobo.h"


typedef struct _machine_hw_spi_obj_t {
    mp_obj_base_t base;
    spi_lobo_bus_config_t buscfg;
    spi_lobo_device_interface_config_t devcfg;
    spi_lobo_device_handle_t spi;
    uint8_t host;
    int8_t polarity;
    int8_t phase;
    int8_t firstbit;
    int8_t duplex;
    enum {
        MACHINE_HW_SPI_STATE_NONE,
        MACHINE_HW_SPI_STATE_INIT,
        MACHINE_HW_SPI_STATE_DEINIT
    } state;
} machine_hw_spi_obj_t;


//--------------------------------------------------------------------
STATIC void machine_hw_spi_deinit_internal(machine_hw_spi_obj_t *self)
{
    if (spi_lobo_bus_remove_device(self->spi) != ESP_OK) {
    	mp_raise_msg(&mp_type_OSError, "error at deinitialization");
    }
}

//---------------------------------------
STATIC void machine_hw_spi_init_internal(
    machine_hw_spi_obj_t    *self,
    int8_t                  host,
    int32_t                 baudrate,
    int8_t                  polarity,
    int8_t                  phase,
    int8_t                  firstbit,
    int8_t                  sck,
    int8_t                  mosi,
    int8_t                  miso,
    int8_t                  cs,
    int8_t                  duplex) {

    // if we're not initialized, then we're implicitly 'changed', since this is the init routine
    uint16_t changed = self->state != MACHINE_HW_SPI_STATE_INIT;

    esp_err_t ret;

    machine_hw_spi_obj_t old_self = *self;

    if ((host > -1) && (host != self->host)) {
        if (host != HSPI_HOST && host != VSPI_HOST) {
            mp_raise_ValueError("SPI host must be either HSPI(1) or VSPI(2)");
        }
        self->host = host;
    	changed |= 0x0004;
    }
    if ((baudrate > 0) && (baudrate != self->devcfg.clock_speed_hz)) {
    	if (baudrate == 0) {
            mp_raise_ValueError("SPI baudrate must be >0");
    	}
    	self->devcfg.clock_speed_hz = baudrate;
    	changed |= 0x0008;
    }
    if ((polarity > -1) && (polarity != self->polarity)) {
        self->polarity = polarity & 1;
    	changed |= 0x0010;
    }
    if ((phase > -1) && (phase != self->phase)) {
        self->phase = phase & 1;
    	changed |= 0x0020;
    }
    if ((firstbit > -1) && (firstbit != self->firstbit)) {
        self->firstbit = firstbit & 1;
    	changed |= 0x0040;
    }
    if ((sck > -1) && (sck != self->buscfg.sclk_io_num)) {
        self->buscfg.sclk_io_num = sck;
    	changed |= 0x0080;
    }
    if ((mosi > -1) && (mosi != self->buscfg.mosi_io_num)) {
        self->buscfg.mosi_io_num = mosi;
    	changed |= 0x0100;
    }
    if ((miso > -1) && (miso != self->buscfg.miso_io_num)) {
        self->buscfg.miso_io_num = miso;
    	changed |= 0x0200;
    }
    if (cs != self->devcfg.spics_ext_io_num) {
        self->devcfg.spics_ext_io_num = cs;
    	changed |= 0x0400;
    }
    if ((duplex > -1) && (duplex != self->duplex)) {
        self->duplex = duplex & 1;
    	changed |= 0x0800;
    }

    if (!changed) return; // no changes
    //printf("[spi] Config changed: %04x\n", changed);

    if (self->state == MACHINE_HW_SPI_STATE_INIT) {
        self->state = MACHINE_HW_SPI_STATE_DEINIT;
        machine_hw_spi_deinit_internal(&old_self);
    }

    self->buscfg.quadwp_io_num = -1;
    self->buscfg.quadhd_io_num = -1;
    self->buscfg.max_transfer_sz = 4096;
    self->devcfg.spics_io_num = -1; // No CS pin

    self->devcfg.mode = self->phase | (self->polarity << 1);
    self->devcfg.flags = ((self->firstbit == MICROPY_PY_MACHINE_SPI_LSB) ? SPI_DEVICE_TXBIT_LSBFIRST | SPI_DEVICE_RXBIT_LSBFIRST : 0);
    if (!self->duplex) self->devcfg.flags |= SPI_DEVICE_HALFDUPLEX;
    self->devcfg.pre_cb = NULL;

    // Add device to spi bus

	ret = spi_lobo_bus_add_device(self->host, &self->buscfg, &self->devcfg, &self->spi);

	if (ret == ESP_OK) {
	    self->state = MACHINE_HW_SPI_STATE_INIT;
	    return;
	}
    switch (ret) {
        case ESP_ERR_INVALID_ARG:
            mp_raise_msg(&mp_type_OSError, "invalid configuration");
            return;
        case ESP_ERR_INVALID_STATE:
            mp_raise_msg(&mp_type_OSError, "invalid state");
            return;
        case ESP_ERR_NO_MEM:
            mp_raise_msg(&mp_type_OSError, "out of memory");
            return;
        case ESP_ERR_NOT_FOUND:
            mp_raise_msg(&mp_type_OSError, "no free slots");
            return;
        default:
			mp_raise_msg(&mp_type_OSError, "error initializing spi");
			return;
    }
}

//-----------------------------------------------------
STATIC mp_obj_t machine_hw_spi_deinit(mp_obj_t self_in)
{
    machine_hw_spi_obj_t *self = self_in;
    if (self->state == MACHINE_HW_SPI_STATE_INIT) {
        self->state = MACHINE_HW_SPI_STATE_DEINIT;
        machine_hw_spi_deinit_internal(self);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_hw_spi_deinit_obj, machine_hw_spi_deinit);

//-----------------------------------------------------------------------------------------------
STATIC void machine_hw_spi_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    machine_hw_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    char scs[16] = {'\0'};
    if (self->devcfg.spics_ext_io_num < 0) sprintf(scs, "Not used");
    else sprintf(scs, "%d", self->devcfg.spics_ext_io_num);

    mp_printf(print, "SPI([%s] spihost=%u, baudrate=%u, polarity=%u, phase=%u, firstbit=%s, sck=%d, mosi=%d, miso=%d, cs=%s, duplex=%s)",
    		  	  	  (self->state == MACHINE_HW_SPI_STATE_INIT) ? "init" : "deinit", self->host,
    				  spi_lobo_get_speed(self->spi), self->polarity, self->phase, (self->firstbit) ? "LSB" : "MSB",
    				  self->buscfg.sclk_io_num, self->buscfg.mosi_io_num, self->buscfg.miso_io_num, scs, (self->duplex) ? "True" : "False");
}

//------------------------------------------------------------------------------------------------
STATIC mp_obj_t machine_hw_spi_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    machine_hw_spi_obj_t *self = pos_args[0];

    enum { ARG_spihost, ARG_baudrate, ARG_polarity, ARG_phase, ARG_firstbit, ARG_sck, ARG_mosi, ARG_miso, ARG_cs, ARD_duplex };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spihost,                   MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_baudrate, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_phase,    MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_sck,      MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mosi,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_miso,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_cs,       MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_duplex,   MP_ARG_KW_ONLY | MP_ARG_INT, {.u_bool = -1} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args-1, pos_args+1, kw_args, MP_ARRAY_SIZE(allowed_args),
                     allowed_args, args);

    int8_t sck=-1, mosi=-1, miso=-1, cs=-1;

    if (args[ARG_sck].u_obj != MP_OBJ_NULL) sck = machine_pin_get_gpio(args[ARG_sck].u_obj);
    if (args[ARG_miso].u_obj != MP_OBJ_NULL) miso = machine_pin_get_gpio(args[ARG_miso].u_obj);
    if (args[ARG_mosi].u_obj != MP_OBJ_NULL) mosi = machine_pin_get_gpio(args[ARG_mosi].u_obj);
    if (args[ARG_cs].u_obj != MP_OBJ_NULL) cs = machine_pin_get_gpio(args[ARG_cs].u_obj);

    machine_hw_spi_init_internal(self, args[ARG_spihost].u_int, args[ARG_baudrate].u_int,
                                 args[ARG_polarity].u_int, args[ARG_phase].u_int,
                                 args[ARG_firstbit].u_int, sck, mosi, miso, cs, args[ARG_firstbit].u_bool);
   return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_hw_spi_init_obj, 0, machine_hw_spi_init);

//---------------------------------------------------------------------------------------------------------------
mp_obj_t machine_hw_spi_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_spihost, ARG_baudrate, ARG_polarity, ARG_phase, ARG_firstbit, ARG_sck, ARG_mosi, ARG_miso, ARG_cs, ARD_duplex };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spihost,  MP_ARG_REQUIRED | MP_ARG_INT , {.u_int = HSPI_HOST} },
        { MP_QSTR_baudrate, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 1000000} },
        { MP_QSTR_polarity, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_phase,    MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_firstbit, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = MICROPY_PY_MACHINE_SPI_MSB} },
        { MP_QSTR_sck,      MP_ARG_KW_ONLY  | MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mosi,     MP_ARG_KW_ONLY  | MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_miso,     MP_ARG_KW_ONLY  | MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_cs,       MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_duplex,   MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 1} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    machine_hw_spi_obj_t *self = m_new_obj(machine_hw_spi_obj_t);
    self->base.type = &machine_hw_spi_type;
    self->state = MACHINE_HW_SPI_STATE_NONE;

    int8_t cs = -1;
    if (args[ARG_cs].u_obj != MP_OBJ_NULL) cs = machine_pin_get_gpio(args[ARG_cs].u_obj);

    machine_hw_spi_init_internal(
        self,
        args[ARG_spihost].u_int,
        args[ARG_baudrate].u_int,
        args[ARG_polarity].u_int,
        args[ARG_phase].u_int,
        args[ARG_firstbit].u_int,
        machine_pin_get_gpio(args[ARG_sck].u_obj),
        machine_pin_get_gpio(args[ARG_mosi].u_obj),
        machine_pin_get_gpio(args[ARG_miso].u_obj),
    	cs,
        args[ARG_firstbit].u_bool);

    return MP_OBJ_FROM_PTR(self);
}

//----------------------------------------------------------------------
STATIC mp_obj_t mp_machine_spi_read(size_t n_args, const mp_obj_t *args)
{
    machine_hw_spi_obj_t *self = args[0];

    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(args[1]));
    memset(vstr.buf, n_args == 3 ? mp_obj_get_int(args[2]) : 0, vstr.len);

	spi_lobo_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction

    t.rxlength = vstr.len * 8;
    t.rx_buffer = vstr.buf;
    if (n_args == 3) {
		t.length = vstr.len * 8;
		t.tx_buffer = vstr.buf;
    }
    else {
		t.length = 0;
		t.tx_buffer = NULL;
    }

	esp_err_t ret = spi_lobo_transfer_data(self->spi, &t);

	if (ret == ESP_OK) {
	    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
	}
    return mp_const_false;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_machine_spi_read_obj, 2, 3, mp_machine_spi_read);

//--------------------------------------------------------------------------
STATIC mp_obj_t mp_machine_spi_readinto(size_t n_args, const mp_obj_t *args)
{
    machine_hw_spi_obj_t *self = args[0];

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_WRITE);
    memset(bufinfo.buf, n_args == 3 ? mp_obj_get_int(args[2]) : 0, bufinfo.len);

	spi_lobo_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction

    t.rxlength = bufinfo.len * 8;
    t.rx_buffer = bufinfo.buf;
    if (n_args == 3) {
		t.length = bufinfo.len * 8;
		t.tx_buffer = bufinfo.buf;
    }
    else {
		t.length = 0;
		t.tx_buffer = NULL;
    }

	esp_err_t ret = spi_lobo_transfer_data(self->spi, &t);

	if (ret == ESP_OK) return mp_const_true;
    return mp_const_false;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_machine_spi_readinto_obj, 2, 3, mp_machine_spi_readinto);


//---------------------------------------------------------------------
STATIC mp_obj_t mp_machine_spi_write(mp_obj_t self_in, mp_obj_t wr_buf)
{
    machine_hw_spi_obj_t *self = self_in;
    mp_buffer_info_t src;
    mp_get_buffer_raise(wr_buf, &src, MP_BUFFER_READ);

	spi_lobo_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction

    t.length = src.len * 8;
    t.tx_buffer = src.buf;
    t.rxlength = 0;
    t.rx_buffer = NULL;

	esp_err_t ret = spi_lobo_transfer_data(self->spi, &t);

	if (ret == ESP_OK) return mp_const_true;
    return mp_const_false;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_machine_spi_write_obj, mp_machine_spi_write);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t mp_machine_spi_write_readinto(mp_obj_t self_in, mp_obj_t wr_buf, mp_obj_t rd_buf)
{
    machine_hw_spi_obj_t *self = self_in;
    mp_buffer_info_t src;
    mp_get_buffer_raise(wr_buf, &src, MP_BUFFER_READ);
    mp_buffer_info_t dest;
    mp_get_buffer_raise(rd_buf, &dest, MP_BUFFER_WRITE);

	spi_lobo_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction

    t.length = src.len * 8;
    t.tx_buffer = src.buf;
    t.rxlength = dest.len;
    t.rx_buffer = dest.buf;

	esp_err_t ret = spi_lobo_transfer_data(self->spi, &t);

	if (ret == ESP_OK) return mp_const_true;
    return mp_const_false;
}
MP_DEFINE_CONST_FUN_OBJ_3(mp_machine_spi_write_readinto_obj, mp_machine_spi_write_readinto);


//================================================================
STATIC const mp_rom_map_elem_t machine_spi_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), (mp_obj_t)&machine_hw_spi_init_obj },
    { MP_ROM_QSTR(MP_QSTR_deinit), (mp_obj_t)&machine_hw_spi_deinit_obj },
    { MP_ROM_QSTR(MP_QSTR_read), (mp_obj_t)&mp_machine_spi_read_obj },
    { MP_ROM_QSTR(MP_QSTR_readinto), (mp_obj_t)&mp_machine_spi_readinto_obj },
    { MP_ROM_QSTR(MP_QSTR_write), (mp_obj_t)&mp_machine_spi_write_obj },
    { MP_ROM_QSTR(MP_QSTR_write_readinto), (mp_obj_t)&mp_machine_spi_write_readinto_obj },

    { MP_ROM_QSTR(MP_QSTR_MSB), MP_ROM_INT(MICROPY_PY_MACHINE_SPI_MSB) },
    { MP_ROM_QSTR(MP_QSTR_LSB), MP_ROM_INT(MICROPY_PY_MACHINE_SPI_LSB) },
	{ MP_ROM_QSTR(MP_QSTR_HSPI), MP_ROM_INT(HSPI_HOST) },
	{ MP_ROM_QSTR(MP_QSTR_VSPI), MP_ROM_INT(VSPI_HOST) },
};
MP_DEFINE_CONST_DICT(mp_machine_spi_locals_dict, machine_spi_locals_dict_table);

//=========================================
const mp_obj_type_t machine_hw_spi_type = {
    { &mp_type_type },
    .name = MP_QSTR_SPI,
    .print = machine_hw_spi_print,
    .make_new = machine_hw_spi_make_new,
    .locals_dict = (mp_obj_dict_t *) &mp_machine_spi_locals_dict,
};
