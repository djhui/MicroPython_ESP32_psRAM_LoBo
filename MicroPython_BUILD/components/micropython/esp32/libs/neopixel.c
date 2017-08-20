
#include "driver/gpio.h"
#include "driver/rmt.h"
#include <math.h>

#include "libs/neopixel.h"


/**
 * Set two levels of RMT output to the Neopixel value for a "1".
 * This is:
 *  * a logic 1 for 0.7us
 *  * a logic 0 for 0.6us
 */
//--------------------------------------------------------------------
static void neopixel_mark(rmt_item32_t *pItem, pixel_settings_t *px) {
	pItem->level0    = px->timings.mark.level0;
	pItem->duration0 = px->timings.mark.duration0;
	pItem->level1    = px->timings.mark.level1;
	pItem->duration1 = px->timings.mark.duration1;
}

/**
 * Set two levels of RMT output to the Neopixel value for a "0".
 * This is:
 *  * a logic 1 for 0.35us
 *  * a logic 0 for 0.8us
 */
//---------------------------------------------------------------------
static void neopixel_space(rmt_item32_t *pItem, pixel_settings_t *px) {
	pItem->level0    = px->timings.space.level0;
	pItem->duration0 = px->timings.space.duration0;
	pItem->level1    = px->timings.space.level1;
	pItem->duration1 = px->timings.space.duration1;
}

//--------------------------------------------------------------------
static void rmt_terminate(rmt_item32_t *pItem, pixel_settings_t *px) {
	pItem->level0    = px->timings.reset.level0;
	pItem->duration0 = px->timings.reset.duration0;
	pItem->level1    = px->timings.reset.level1;
	pItem->duration1 = px->timings.reset.duration1;
}

//----------------------------------------
uint8_t offset_color(char o, pixel_t *p) {
	switch(o) {
		case 'R': return p->red;
		case 'B': return p->green;
		case 'G': return p->blue;
		case 'W': return p->white;
	}
	return 0;
}

//--------------------------------------------------------------------
static uint32_t get_wire_value(pixel_settings_t *px, uint16_t pixel) {
	return  (offset_color(px->color_order[0], &px->pixels[pixel]) << 24) |
		(offset_color(px->color_order[1], &px->pixels[pixel]) << 16) |
		(offset_color(px->color_order[2], &px->pixels[pixel]) << 8)  |
		(offset_color(px->color_order[3], &px->pixels[pixel]));
}

//----------------------------------------------------------------
void np_set_color_order(pixel_settings_t *px, pixel_order_t order)
{
	px->white_offset = (order >> 6) & 0b11;
	px->red_offset   = (order >> 4) & 0b11;
	px->green_offset = (order >> 2) & 0b11;
	px->blue_offset  = order & 0b11;
}

//------------------------------------------------------------------------------------------------------------------
void np_set_pixel_color(pixel_settings_t *px, uint16_t idx, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
	px->pixels[idx].red   = (uint16_t)(red * px->brightness) / 255;
	px->pixels[idx].green = (uint16_t)(green * px->brightness) / 255;
	px->pixels[idx].blue  = (uint16_t)(blue * px->brightness) / 255;
	px->pixels[idx].white = (uint16_t)(white * px->brightness) / 255;
}

//---------------------------------------------------------------------------
void np_set_pixel_color32(pixel_settings_t *px, uint16_t idx, uint32_t color)
{
	px->pixels[idx].red   = (uint16_t)(((color >> 24) & 0xFF) * px->brightness) / 255;
	px->pixels[idx].green = (uint16_t)(((color >> 16) & 0xFF) * px->brightness) / 255;
	px->pixels[idx].blue  = (uint16_t)(((color >> 8) & 0xFF) * px->brightness) / 255;
	px->pixels[idx].white = (uint16_t)((color && 0xFF) * px->brightness) / 255;
}

//------------------------------------------------------------------------------------------------------------
void np_set_pixel_color_hsb(pixel_settings_t *px, uint16_t idx, float hue, float saturation, float brightness)
{
	uint32_t color = hsb_to_rgb(hue, saturation, brightness);
	np_set_pixel_color32(px, idx, color);
}

//----------------------------------------------------------------------------------------------------------------------
void np_get_pixel_color(pixel_settings_t *px, uint16_t idx, uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *white)
{
	*red = px->pixels[idx].red;
	*green = px->pixels[idx].green;
	*blue = px->pixels[idx].blue;
	*white = px->pixels[idx].white;
}

uint32_t np_get_pixel_color32(pixel_settings_t *px, uint16_t idx)
{
	return (uint32_t)((px->pixels[idx].red << 24) || (px->pixels[idx].green << 16) || (px->pixels[idx].blue << 8) || (px->pixels[idx].white));
}

//--------------------------------
void np_show(pixel_settings_t *px)
{
  rmt_item32_t * pCurrentItem = px->items;

  for (uint16_t i = 0; i < px->pixel_count; i++) {
    uint32_t p = get_wire_value(px, i);

    for (int j=31; j>=(32-px->nbits); j--) {
      // 32/24 bits of data represent the red, green, blue and (white) channels. The
      // value of the 32/24 bits to output is in the variable p. This value must
      // be written to the RMT subsystem in big-endian format. Iterate through
      // the pixels MSB to LSB
      if (p & (1<<j)) {
        neopixel_mark(pCurrentItem, px);
      } else {
        neopixel_space(pCurrentItem, px);
      }
      pCurrentItem++;
    }
  }

  rmt_terminate(pCurrentItem, px);

  ESP_ERROR_CHECK(rmt_write_items(RMT_CHANNEL_0, px->items, px->pixel_count * 32, 1));
}

//---------------------------------
void np_clear(pixel_settings_t *px)
{
	for(size_t i = 0; i < px->pixel_count; ++i) {
		np_set_pixel_color(px, i, 0, 0, 0, 0);
	}
}

//------------------------------------
static float Min(double a, double b) {
	return a <= b ? a : b;
}

//------------------------------------
static float Max(double a, double b) {
	return a >= b ? a : b;
}

//-------------------------------------------------------------------
void rgb_to_hsb( uint32_t color, float *hue, float *sat, float *bri )
{
	float delta, min;
	float h = 0, s, v;
	uint8_t red = (color >> 24) & 0xFF;
	uint8_t green = (color >> 16) & 0xFF;
	uint8_t blue = (color >> 8) & 0xFF;

	min = Min(Min(red, green), blue);
	v = Max(Max(red, green), blue);
	delta = v - min;

	if (v == 0.0) s = 0;
	else s = delta / v;

	if (s == 0)	h = 0.0;
	else
	{
		if (red == v)
			h = (green - blue) / delta;
		else if (green == v)
			h = 2 + (blue - red) / delta;
		else if (blue == v)
			h = 4 + (red - green) / delta;

		h *= 60;

		if (h < 0.0) h = h + 360;
	}

	*hue = h;
	*sat = s;
	*bri = v / 255;
}

//------------------------------------------------------------
uint32_t hsb_to_rgb(float _hue, float _sat, float _brightness)
{
	float red = 0.0;
	float green = 0.0;
	float blue = 0.0;

	if (_sat == 0.0) {
		red = _brightness;
		green = _brightness;
		blue = _brightness;
	}
	else {
		if (_hue >= 360.0) _hue = fmod(_hue, 360);

		int slice = (int)(_hue / 60.0);
		float hue_frac = (_hue / 60.0) - slice;

		float aa = _brightness * (1.0 - _sat);
		float bb = _brightness * (1.0 - _sat * hue_frac);
		float cc = _brightness * (1.0 - _sat * (1.0 - hue_frac));

		switch(slice) {
			case 0:
				red = _brightness;
				green = cc;
				blue = aa;
				break;
			case 1:
				red = bb;
				green = _brightness;
				blue = aa;
				break;
			case 2:
				red = aa;
				green = _brightness;
				blue = cc;
				break;
			case 3:
				red = aa;
				green = bb;
				blue = _brightness;
				break;
			case 4:
				red = cc;
				green = aa;
				blue = _brightness;
				break;
			case 5:
				red = _brightness;
				green = aa;
				blue = bb;
				break;
			default:
				red = 0.0;
				green = 0.0;
				blue = 0.0;
				break;
		}
	}

	return ((uint8_t)(red * 255.0) << 24) | ((uint8_t)(green * 255.0) << 16) | ((uint8_t)(blue * 255.0) << 8);
}

