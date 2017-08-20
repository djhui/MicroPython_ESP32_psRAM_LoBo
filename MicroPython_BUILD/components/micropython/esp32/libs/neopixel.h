#pragma once

#include "driver/gpio.h"
#include "driver/rmt.h"

#define DIVIDER             4 // 80 MHz clock divider
#define RMT_DURATION_NS  12.5 // minimum time of a single RMT duration based on 80 MHz clock (ns)
#define RMT_PERIOD_NS      50 // minimum bit time based on 80 MHz clock and divider of 4


typedef struct pixel {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t white;
} pixel_t;

typedef struct bit_timing {
	uint8_t level0;
	uint16_t duration0;
	uint8_t level1;
	uint16_t duration1;
} bit_timing_t;

typedef struct pixel_timing {
	bit_timing_t mark;
	bit_timing_t space;
	bit_timing_t reset;
} pixel_timing_t;

typedef struct pixel_settings {
	pixel_t *pixels;		// buffer containing pixel values
	rmt_item32_t *items;	// buffer containing BIT values for all the pixels
	pixel_timing_t timings;	// timing data from which the pixels BIT data are formed
	uint16_t pixel_count;	// number of used pixels
	uint8_t brightness;		// brightness factor applied to pixel color
	char color_order[5];
	int8_t red_offset;
	int8_t blue_offset;
	int8_t green_offset;
	int8_t white_offset;
	uint8_t nbits;			// number of bits used (24 for RGB devices, 32 for RGBW devices)
} pixel_settings_t;

#define NEOPIXEL_INIT_CONFIG_DEFAULT {\
	.pixel_count = 0, \
	.brightness = 255,\
	.color_order = "BRGW",\
	.red_offset = 24,\
	.green_offset = 16,\
	.blue_offset = 8,\
	.white_offset = 0,\
	.nbits = 24\
}

#define NEOPIXEL_RMT_INIT_CONFIG_DEFAULT {\
	.rmt_mode = RMT_MODE_TX,\
	.channel = RMT_CHANNEL_0,\
	.gpio_num = 0,\
	.mem_block_num = 8 - RMT_CHANNEL_0,\
	.clk_div = DIVIDER,\
	.tx_config.loop_en = 0,\
	.tx_config.carrier_en = 0,\
	.tx_config.idle_output_en = 1,\
	.tx_config.idle_level = (rmt_idle_level_t)0,\
	.tx_config.carrier_freq_hz = 10000,\
	.tx_config.carrier_level = (rmt_carrier_level_t)1,\
	.tx_config.carrier_duty_percent = 50\
}

#define DEFAULT_WS2812_TIMINGS {\
		.mark = {\
			.level0    = 1,\
			.duration0 = 12,\
			.level1    = 0,\
			.duration1 = 14\
		},\
		.space = {\
			.level0    = 1,\
			.duration0 = 7,\
			.level1    = 0,\
			.duration1 = 16\
		},\
		.reset = {\
			.level0    = 0,\
			.duration0 = 600,\
			.level1    = 0,\
			.duration1 = 600\
		}\
}

typedef uint8_t pixel_order_t;

//const pixel_timing_t np_type_ws2812;

void np_set_color_order(pixel_settings_t *px, pixel_order_t order);
void np_set_pixel_color(pixel_settings_t *px, uint16_t idx, uint8_t red, uint8_t green, uint8_t blue, uint8_t white);
void np_set_pixel_color32(pixel_settings_t *px, uint16_t idx, uint32_t color);
void np_set_pixel_color_hsb(pixel_settings_t *px, uint16_t idx, float hue, float saturation, float brightness);
void np_get_pixel_color(pixel_settings_t *px, uint16_t idx, uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *white);
uint32_t np_get_pixel_color32(pixel_settings_t *px, uint16_t idx);
void np_show(pixel_settings_t *px);
void np_clear(pixel_settings_t *px);

void rgb_to_hsb( uint32_t color, float *hue, float *sat, float *bri );
uint32_t hsb_to_rgb(float hue, float saturation, float brightness);
