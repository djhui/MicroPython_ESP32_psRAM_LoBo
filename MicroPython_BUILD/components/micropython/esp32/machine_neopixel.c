/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Boris Lovosevic (https://github.loboris)
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

/*
 * Module for the Neopixel/WS2812 RGB LEDs using the RMT peripheral on the ESP32.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "libs/neopixel.h"

#include "py/nlr.h"
#include "py/runtime.h"
#include "modmachine.h"
#include "mphalport.h"


typedef struct _machine_neopixel_obj_t {
    mp_obj_base_t base;
    rmt_config_t config;
    pixel_settings_t px;
} machine_neopixel_obj_t;


//------------------------------------------------
STATIC void np_check(machine_neopixel_obj_t *self)
{
    if ((self->px.pixels == NULL) || (self->px.items == NULL)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Neopixel instance not initialized"));
    }
}

//-----------------------------------------------------------------------------------------------
STATIC void machine_neopixel_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    machine_neopixel_obj_t *self = self_in;

    if ((self->px.pixels != NULL) && (self->px.items != NULL)) {
		mp_printf(print, "Neopixel(Pin=%d, Pixels: %d, RMTChannel=%d, PixBufLen=%u, BitBufLen=%u\n",
				self->config.gpio_num, self->px.pixel_count, self->config.channel,
				sizeof(pixel_t) * self->px.pixel_count, sizeof(rmt_item32_t) * ((self->px.pixel_count * 32) + 1));
		mp_printf(print, "         Timings ns: BIT1: (%d, %d, %d, %d), BIT0: (%d, %d, %d, %d), RST: %d\n         )",
				self->px.timings.mark.level0, self->px.timings.mark.duration0 * RMT_PERIOD_NS, self->px.timings.mark.level1, self->px.timings.mark.duration1 * RMT_PERIOD_NS,
				self->px.timings.space.level0, self->px.timings.space.duration0 * RMT_PERIOD_NS, self->px.timings.space.level1, self->px.timings.space.duration1 * RMT_PERIOD_NS,
				(self->px.timings.reset.duration0 + self->px.timings.reset.duration1) * RMT_PERIOD_NS);
    }
    else {
		mp_printf(print, "Neopixel(Not initialized)");
    }
}

//------------------------------------------------------------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
	enum { ARG_pin, ARG_pixels, ARG_type, ARG_rmtchan };
	//------------------------------------------------------------
	const mp_arg_t machine_neopixel_init_allowed_args[] = {
			{ MP_QSTR_pin,     MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
			{ MP_QSTR_pixels,  MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 4} },
			{ MP_QSTR_type,                      MP_ARG_INT, {.u_int = 0} },
			{ MP_QSTR_rmtchan, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
	};

	mp_arg_val_t args[MP_ARRAY_SIZE(machine_neopixel_init_allowed_args)];
	mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(machine_neopixel_init_allowed_args), machine_neopixel_init_allowed_args, args);

    int8_t pin = machine_pin_get_id(args[ARG_pin].u_obj);
    int pixels = args[ARG_pixels].u_int;
    int rmtchan = args[ARG_rmtchan].u_int;
    int wstype = args[ARG_type].u_int;

    // Check type
    if ((wstype < 0) || (wstype > 4)) {
    	mp_raise_ValueError("Wrong type");
    }
    // Check the rmt channel
    if (rmtchan < 0 || rmtchan > 7) {
    	mp_raise_ValueError("RMT interface channel not valid, should be 0~7");
    }
    // Check pin
    if (pin > 31) {
    	mp_raise_ValueError("Wrong pin, only pins 0~31 allowed");
    }
    // Check number of pixels
    if ((pixels < 0) || (pixels > 255)) {
    	mp_raise_ValueError("Maximum 255 pixels can be used");
    }

    // Setup the neopixels object
    machine_neopixel_obj_t *self = m_new_obj(machine_neopixel_obj_t );

    pixel_settings_t px = NEOPIXEL_INIT_CONFIG_DEFAULT;
    pixel_timing_t timings = DEFAULT_WS2812_TIMINGS;
    rmt_config_t cfg = NEOPIXEL_RMT_INIT_CONFIG_DEFAULT;

    memcpy(&self->px, &px, sizeof(pixel_settings_t));
    memcpy(&self->px.timings, &timings, sizeof(pixel_timing_t));
    memcpy(&self->config, &cfg, sizeof(rmt_config_t));

    self->px.pixel_count = pixels;
    self->config.channel = RMT_CHANNEL_0;
    self->config.gpio_num = pin;

    // Allocate buffers
    self->px.pixels = m_new(pixel_t, sizeof(pixel_t) * self->px.pixel_count);
    //self->px.pixels = malloc(sizeof(pixel_t) * self->px.pixel_count);
    if (self->px.pixels == NULL) goto error_exit;
    self->px.items = m_new(rmt_item32_t, sizeof(rmt_item32_t) * ((self->px.pixel_count * 32) + 1));
    //self->px.items = malloc(sizeof(rmt_item32_t) * ((self->px.pixel_count * 32) + 1));
    if (self->px.items == NULL) goto error_exit;

    esp_err_t res = rmt_config(&self->config);
    if (res != ESP_OK) goto error_exit;
	res = rmt_driver_install(self->config.channel, 0, 0);
    if (res != ESP_OK) goto error_exit;

    self->base.type = &machine_neopixel_type;

	np_clear(&self->px);
	np_show(&self->px);

    return MP_OBJ_FROM_PTR(self);

error_exit:
	if (self->px.pixels) m_del(pixel_t, self->px.pixels, 1);
	if (self->px.items) m_del(rmt_item32_t, self->px.items, 1);
	//if (self->px.pixels) free(self->px.pixels);
	//if (self->px.items) free(self->px.items);
	self->px.pixels = NULL;
	self->px.items = NULL;

    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Error initializing Neopixel interface"));
	return mp_const_none;
}

//------------------------------------------------------
STATIC mp_obj_t machine_neopixel_deinit(mp_obj_t self_in)
{
    machine_neopixel_obj_t *self = self_in;
    np_check(self);

	np_clear(&self->px);
	np_show(&self->px);

	if (self->px.pixels) m_del(pixel_t, self->px.pixels, 1);
    if (self->px.items) m_del(rmt_item32_t, self->px.items, 1);
	//if (self->px.pixels) free(self->px.pixels);
    //if (self->px.items) free(self->px.items);
    rmt_driver_uninstall(self->config.channel);

    pixel_settings_t px = NEOPIXEL_INIT_CONFIG_DEFAULT;
    memcpy(&self->px, &px, sizeof(pixel_settings_t));
    rmt_config_t cfg = NEOPIXEL_RMT_INIT_CONFIG_DEFAULT;
    memcpy(&self->config, &cfg, sizeof(rmt_config_t));

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_neopixel_deinit_obj, machine_neopixel_deinit);

//------------------------------------------------------
STATIC mp_obj_t machine_neopixel_clear(mp_obj_t self_in)
{
    machine_neopixel_obj_t *self = self_in;
    np_check(self);

	np_clear(&self->px);
   	MP_THREAD_GIL_EXIT();
	np_show(&self->px);
   	MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_neopixel_clear_obj, machine_neopixel_clear);

//-----------------------------------------------------
STATIC mp_obj_t machine_neopixel_show(mp_obj_t self_in)
{
    machine_neopixel_obj_t *self = self_in;

   	MP_THREAD_GIL_EXIT();
	np_show(&self->px);
   	MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_neopixel_show_obj, machine_neopixel_show);

//----------------------------------------------------
STATIC mp_obj_t machine_neopixel_set(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
	const mp_arg_t allowed_args[] = {
	    { MP_QSTR_pos,   MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
	    { MP_QSTR_color, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
	    { MP_QSTR_num,                     MP_ARG_OBJ, {.u_int = 1} },
	    { MP_QSTR_update,                  MP_ARG_OBJ, {.u_bool = true} },
	};
	machine_neopixel_obj_t *self = pos_args[0];
    np_check(self);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    int cnt = args[2].u_int & 0xFF;
    uint32_t color = (uint32_t)args[1].u_int & 0x00FFFFFF;
    int pos = args[0].u_int & 0xFF;

	if (pos < 1) pos = 1;
	if (pos > self->px.pixel_count) pos = self->px.pixel_count;
	if (cnt < 1) cnt = 1;
	if ((cnt + pos - 1) > self->px.pixel_count) cnt = self->px.pixel_count - pos + 1;

	for (uint8_t i = 0; i < cnt; i++) {
		np_set_pixel_color32(&self->px, i+pos-1, color << 8);
	}

	if (args[3].u_bool) {
	   	MP_THREAD_GIL_EXIT();
		np_show(&self->px);
	   	MP_THREAD_GIL_ENTER();
	}

	return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_neopixel_set_obj, 2, machine_neopixel_set);

//---------------------------------------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_setHSB(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
  	    { MP_QSTR_pos, 		   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0} },
        { MP_QSTR_hue,         MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_saturation,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_brightness,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
	    { MP_QSTR_num,                           MP_ARG_OBJ, { .u_int = 1} },
	    { MP_QSTR_update,                        MP_ARG_OBJ, { .u_bool = true} },
    };
	machine_neopixel_obj_t *self = pos_args[0];
    np_check(self);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_float_t hue = mp_obj_get_float(args[1].u_obj);
    mp_float_t sat = mp_obj_get_float(args[2].u_obj);
    mp_float_t bri = mp_obj_get_float(args[3].u_obj);

    uint32_t color = hsb_to_rgb(hue, sat, bri);

    int pos = args[0].u_int & 0xFF;
    int cnt = args[4].u_int & 0xFF;

	if (pos < 1) pos = 1;
	if (pos > self->px.pixel_count) pos = self->px.pixel_count;
	if (cnt < 1) cnt = 1;
	if ((cnt + pos - 1) > self->px.pixel_count) cnt = self->px.pixel_count - pos + 1;

	for (uint8_t i = 0; i < cnt; i++) {
		np_set_pixel_color32(&self->px, i+pos-1, color);
	}

	if (args[3].u_bool) {
	   	MP_THREAD_GIL_EXIT();
		np_show(&self->px);
	   	MP_THREAD_GIL_ENTER();
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_neopixel_setHSB_obj, 3, machine_neopixel_setHSB);

//---------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_get(mp_obj_t self_in, mp_obj_t pos_in)
{
    machine_neopixel_obj_t *self = self_in;
    np_check(self);

    int pos = mp_obj_get_int(pos_in);
    if (pos < 1) pos = 1;
    if (pos > self->px.pixel_count) pos = self->px.pixel_count;

    uint32_t icolor = np_get_pixel_color32(&self->px, pos-1) >> 8;

    return mp_obj_new_int(icolor);
}
MP_DEFINE_CONST_FUN_OBJ_2(machine_neopixel_get_obj, machine_neopixel_get);

//----------------------------------------------------
STATIC mp_obj_t machine_neopixel_brightness(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
	const mp_arg_t allowed_args[] = {
	    { MP_QSTR_brightness,   MP_ARG_INT, {.u_int = -1} },
	};
	machine_neopixel_obj_t *self = pos_args[0];
    np_check(self);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    int bright = args[0].u_int;
    if (bright > 0) {
    	self->px.brightness = bright & 0xFF;
    }
    return mp_obj_new_int(self->px.brightness);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_neopixel_brightness_obj, 0, machine_neopixel_brightness);

//-----------------------------------------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_HSBtoRGB(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_hue,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_saturation,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_brightness,  MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
	machine_neopixel_obj_t *self = pos_args[0];
    np_check(self);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_float_t hue = mp_obj_get_float(args[0].u_obj);
    mp_float_t sat = mp_obj_get_float(args[1].u_obj);
    mp_float_t bri = mp_obj_get_float(args[2].u_obj);

    uint32_t color = hsb_to_rgb(hue, sat, bri);

    return mp_obj_new_int(color >> 8);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_neopixel_HSBtoRGB_obj, 3, machine_neopixel_HSBtoRGB);

//------------------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_RGBtoHSB(mp_obj_t self_in, mp_obj_t color_in) {

    machine_neopixel_obj_t *self = self_in;
    np_check(self);

    uint32_t color = mp_obj_get_int(color_in);

    float hue, sat, bri;

    rgb_to_hsb(color << 8, &hue, &sat, &bri);

   	mp_obj_t tuple[3];

   	tuple[0] = mp_obj_new_float(hue);
   	tuple[1] = mp_obj_new_float(sat);
   	tuple[2] = mp_obj_new_float(bri);

   	return mp_obj_new_tuple(3, tuple);
}
MP_DEFINE_CONST_FUN_OBJ_2(machine_neopixel_RGBtoHSB_obj, machine_neopixel_RGBtoHSB);

//---------------------------------------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_corder(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_color_order,  MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
	machine_neopixel_obj_t *self = pos_args[0];
    np_check(self);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    const char *corder = NULL;
    if (MP_OBJ_IS_STR(args[0].u_obj)) {
    	corder = mp_obj_str_get_str(args[0].u_obj);
    	int len = strlen(corder);
    	if ((len >= 3) || (len > 4)) {
    		if (((strchr(corder, 'W') == NULL) || (strchr(corder, 'W') == corder+3)) &&
    		    ((strchr(corder, 'R') != NULL) && (strchr(corder, 'G') != NULL) && (strchr(corder, 'B') != NULL))) {
    			if (len == 3) strcat(corder, "W");
    			strcpy(self->px.color_order, corder);
    		}
    	}
    }
    return mp_obj_new_str(self->px.color_order, strlen(self->px.color_order), 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_neopixel_corder_obj, 0, machine_neopixel_corder);

//----------------------------------------------------------------------------------------------------
STATIC mp_obj_t machine_neopixel_timings(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_timings, MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
	machine_neopixel_obj_t *self = pos_args[0];
    np_check(self);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (MP_OBJ_IS_TYPE(args[0].u_obj, &mp_type_tuple)) {
        mp_obj_t *t_items;
        mp_obj_t *t1_items;
        mp_obj_t *t0_items;
        uint t_len, t1_len, t0_len;
        int rst;

        mp_obj_tuple_get(args[0].u_obj, &t_len, &t_items);
        if (t_len == 3) {
			mp_obj_tuple_get(t_items[0], &t1_len, &t1_items);
			mp_obj_tuple_get(t_items[1], &t0_len, &t0_items);
			rst = mp_obj_get_int(t_items[2]) / RMT_PERIOD_NS;
	        if ((t1_len != 4) || (t0_len != 4)) {
	            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Wrong arguments"));
	        }
        }
        else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "tuple argument expected"));
        }
        pixel_timing_t tm = DEFAULT_WS2812_TIMINGS;
        if (rst > 0) {
			tm.mark.level1 = mp_obj_get_int(t1_items[0]) & 1;
			tm.mark.duration1 = (mp_obj_get_int(t1_items[1]) & 0x7FFF) / RMT_PERIOD_NS;
			tm.mark.level0 = mp_obj_get_int(t1_items[2]) & 1;
			tm.mark.duration0 = (mp_obj_get_int(t1_items[3]) & 0x7FFF) / RMT_PERIOD_NS;

			tm.space.level1 = mp_obj_get_int(t0_items[0]) & 1;
			tm.space.duration1 = (mp_obj_get_int(t0_items[1]) & 0x7FFF) / RMT_PERIOD_NS;
			tm.space.level0 = mp_obj_get_int(t0_items[2]) & 1;
			tm.space.duration0 = (mp_obj_get_int(t0_items[3]) & 0x7FFF) / RMT_PERIOD_NS;

			tm.reset.level1 = 0;
			tm.reset.duration1 = rst / 2;
			tm.reset.level0 = 0;
			tm.reset.duration0 = rst / 2;
        }
        memcpy(&self->px.timings, &tm, sizeof(pixel_timing_t));
    }

    mp_obj_t t1_tuple[4];
   	mp_obj_t t0_tuple[4];
   	mp_obj_t t_tuple[3];

   	t1_tuple[0] = mp_obj_new_int(self->px.timings.mark.level0);
   	t1_tuple[1] = mp_obj_new_int(self->px.timings.mark.duration0 * RMT_PERIOD_NS);
   	t1_tuple[2] = mp_obj_new_int(self->px.timings.mark.level1);
   	t1_tuple[3] = mp_obj_new_int(self->px.timings.mark.duration1 * RMT_PERIOD_NS);

   	t0_tuple[0] = mp_obj_new_int(self->px.timings.space.level0);
   	t0_tuple[1] = mp_obj_new_int(self->px.timings.space.duration0 * RMT_PERIOD_NS);
   	t0_tuple[2] = mp_obj_new_int(self->px.timings.space.level1);
   	t0_tuple[3] = mp_obj_new_int(self->px.timings.space.duration1 * RMT_PERIOD_NS);

   	t_tuple[0] = mp_obj_new_tuple(4, t1_tuple);
   	t_tuple[1] = mp_obj_new_tuple(4, t0_tuple);
   	t_tuple[2] = mp_obj_new_int((self->px.timings.reset.duration0 + self->px.timings.reset.duration1) * RMT_PERIOD_NS);

   	return mp_obj_new_tuple(3, t_tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_neopixel_timings_obj, 0, machine_neopixel_timings);


//=====================================================================
STATIC const mp_rom_map_elem_t machine_neopixel_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_clear),      (mp_obj_t)&machine_neopixel_clear_obj },
    { MP_ROM_QSTR(MP_QSTR_set),        (mp_obj_t)&machine_neopixel_set_obj },
    { MP_ROM_QSTR(MP_QSTR_setHSB),     (mp_obj_t)&machine_neopixel_setHSB_obj },
    { MP_ROM_QSTR(MP_QSTR_get),        (mp_obj_t)&machine_neopixel_get_obj },
    { MP_ROM_QSTR(MP_QSTR_show),       (mp_obj_t)&machine_neopixel_show_obj },
    { MP_ROM_QSTR(MP_QSTR_brightness), (mp_obj_t)&machine_neopixel_brightness_obj },
    { MP_ROM_QSTR(MP_QSTR_HSBtoRGB),   (mp_obj_t)&machine_neopixel_HSBtoRGB_obj },
    { MP_ROM_QSTR(MP_QSTR_RGBtoHSB),   (mp_obj_t)&machine_neopixel_RGBtoHSB_obj },
    { MP_ROM_QSTR(MP_QSTR_deinit),     (mp_obj_t)&machine_neopixel_deinit_obj },
    { MP_ROM_QSTR(MP_QSTR_timings),    (mp_obj_t)&machine_neopixel_timings_obj },
    { MP_ROM_QSTR(MP_QSTR_color_order),(mp_obj_t)&machine_neopixel_corder_obj },

	{ MP_ROM_QSTR(MP_QSTR_BLACK), MP_ROM_INT(0x000000) },
	{ MP_ROM_QSTR(MP_QSTR_WHITE), MP_ROM_INT(0xFFFFFF) },
	{ MP_ROM_QSTR(MP_QSTR_RED), MP_ROM_INT(0xFF0000) },
	{ MP_ROM_QSTR(MP_QSTR_LIME), MP_ROM_INT(0x00FF00) },
	{ MP_ROM_QSTR(MP_QSTR_BLUE), MP_ROM_INT(0x0000FF) },
	{ MP_ROM_QSTR(MP_QSTR_YELLOW), MP_ROM_INT(0xFFFF00) },
	{ MP_ROM_QSTR(MP_QSTR_CYAN), MP_ROM_INT(0x00FFFF) },
	{ MP_ROM_QSTR(MP_QSTR_MAGENTA), MP_ROM_INT(0xFF00FF) },
	{ MP_ROM_QSTR(MP_QSTR_SILVER), MP_ROM_INT(0xC0C0C0) },
	{ MP_ROM_QSTR(MP_QSTR_GRAY), MP_ROM_INT(0x808080) },
	{ MP_ROM_QSTR(MP_QSTR_MAROON), MP_ROM_INT(0x800000) },
	{ MP_ROM_QSTR(MP_QSTR_OLIVE), MP_ROM_INT(0x808000) },
	{ MP_ROM_QSTR(MP_QSTR_GREEN), MP_ROM_INT(0x008000) },
	{ MP_ROM_QSTR(MP_QSTR_PURPLE), MP_ROM_INT(0x800080) },
	{ MP_ROM_QSTR(MP_QSTR_TEAL), MP_ROM_INT(0x008080) },
	{ MP_ROM_QSTR(MP_QSTR_NAVY), MP_ROM_INT(0x000080) },
};

STATIC MP_DEFINE_CONST_DICT(machine_neopixel_locals_dict, machine_neopixel_locals_dict_table);

//===========================================
const mp_obj_type_t machine_neopixel_type = {
    { &mp_type_type },
    .name = MP_QSTR_Neopixel,
    .print = machine_neopixel_print,
    .make_new = machine_neopixel_make_new,
    .locals_dict = (mp_obj_dict_t*)&machine_neopixel_locals_dict,
};

