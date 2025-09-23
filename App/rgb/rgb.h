/*
 * rgb.h
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */

#ifndef RGB_RGB_H_
#define RGB_RGB_H_


#include "def.h"


#define RGB_SHAPE_V__R		GPIO_PIN_1
#define RGB_SHAPE_V__G		GPIO_PIN_2
#define RGB_SHAPE_V__B		GPIO_PIN_0

typedef enum
{
	RGB_ZONE_V_SHAPE = 0,
	RGB_ZONE_EYES,
	RGB_ZONE_COUNT
} rgb_zone_t;


typedef enum
{
	COLOR_RED,
	COLOR_ORANGE,
	COLOR_YELLOW,
	COLOR_GREEN,
	COLOR_BLUE,
	COLOR_PURPLE,
	COLOR_LIGHT_GREEN,
	COLOR_SKY_BLUE,
	COLOR_PINK,
	COLOR_BLACK,
	COLOR_WHITE,
	COLOR_GRAY,
	COLOR_COUNT,
}color_t;


typedef struct
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
}rgb_led_t;


void rgb_init(void);
void rgb_set_color(rgb_zone_t zone, color_t color);
void rgb_set_rgb(rgb_zone_t zone, uint8_t r, uint8_t g, uint8_t b);
void rgb_tick(void);   // 타이머 ISR(또는 주기 함수)에서 호출


#endif /* RGB_RGB_H_ */
