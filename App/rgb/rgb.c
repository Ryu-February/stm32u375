/*
 * rgb.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "rgb.h"


const rgb_led_t led_map[COLOR_COUNT] =
{
		[COLOR_RED]         = { 255,   0,   0 },
		[COLOR_ORANGE]      = { 255, 165,   0 },
		[COLOR_YELLOW]      = { 255, 255,   0 },
		[COLOR_GREEN]       = {   0, 255,   0 },
		[COLOR_BLUE]        = {   0,   0, 255 },
		[COLOR_PURPLE]      = { 160,  32, 240 },  // or VIOLET
		[COLOR_LIGHT_GREEN] = {  26, 255,  26 },
		[COLOR_SKY_BLUE]    = {  70, 200, 255 },  // LIGHT_BLUE or CYAN mix
		[COLOR_PINK]        = { 255, 105, 180 },
		[COLOR_BLACK]       = {   0,   0,   0 },
		[COLOR_WHITE]       = { 255, 255, 255 },
		[COLOR_GRAY]        = { 128, 128, 128 }
};


/* ───────────────── 핀 매핑 (Active-Low: LOW=ON) ─────────────────
 * V_SHAPE : PC0(B), PC1(R), PC2(G)
 * EYES    : PC10(B), PC11(R), PC12(G)
 */

#define LED_PORT			GPIOC

#define V_B_PIN             GPIO_PIN_0		//shape - v(top pcb)
#define V_R_PIN             GPIO_PIN_1
#define V_G_PIN             GPIO_PIN_2

#define E_B_PIN             GPIO_PIN_10		//shape - eyes
#define E_R_PIN             GPIO_PIN_11
#define E_G_PIN             GPIO_PIN_12


/* 채널 인덱스 */
enum { CH_R = 0, CH_G = 1, CH_B = 2 };


/* 듀티 버퍼: [ZONE][CH] */
static volatile uint8_t duty[RGB_ZONE_COUNT][3] = {0};

/* 공용 PWM 카운터(0~254) */
static volatile uint8_t pwm_counter = 0;

/* Active-Low 보정된 핀 쓰기 */
static inline void write_pin_active_low(uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(LED_PORT, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}


/* 하나의 ZONE에 대해 세 채널을 즉시 반영 */
static inline void apply_zone(rgb_zone_t zone, uint8_t r_on, uint8_t g_on, uint8_t b_on)
{
    if (zone == RGB_ZONE_V_SHAPE)
    {
        write_pin_active_low(V_R_PIN, r_on);
        write_pin_active_low(V_G_PIN, g_on);
        write_pin_active_low(V_B_PIN, b_on);
    }
    else	/* RGB_ZONE_EYES */
    {
        write_pin_active_low(E_R_PIN, r_on);
        write_pin_active_low(E_G_PIN, g_on);
        write_pin_active_low(E_B_PIN, b_on);
    }
}

/* ───────────────── Public API ───────────────── */

void rgb_init(void)
{
	HAL_GPIO_WritePin(LED_PORT,
	                      V_R_PIN | V_G_PIN | V_B_PIN |
	                      E_R_PIN | E_G_PIN | E_B_PIN,
	                      GPIO_PIN_SET);

	/* 기본 듀티 0 */
	for (int Z = 0; Z < RGB_ZONE_COUNT; ++Z)
	{
		duty[Z][CH_R] = 0;
		duty[Z][CH_G] = 0;
		duty[Z][CH_B] = 0;
	}
	pwm_counter = 0;
}

void rgb_set_color(rgb_zone_t zone, color_t color)
{
	if(color >= COLOR_COUNT)
		return;

    const rgb_led_t *c = &led_map[color];

    duty[zone][CH_R] = c->r;
    duty[zone][CH_G] = c->g;
    duty[zone][CH_B] = c->b;
}

void rgb_set_rgb(rgb_zone_t zone, uint8_t r, uint8_t g, uint8_t b)
{
    duty[zone][CH_R] = r;
    duty[zone][CH_G] = g;
    duty[zone][CH_B] = b;
}


// 타이머 주기마다 호출(예: 타이머 주파수/255 = 200~1000Hz 영역)
void rgb_tick(void)
{
    pwm_counter++;

    // V_SHAPE
    {
        uint8_t r_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_V_SHAPE][CH_R]));
        uint8_t g_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_V_SHAPE][CH_G]));
        uint8_t b_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_V_SHAPE][CH_B]));
        apply_zone(RGB_ZONE_V_SHAPE, r_on, g_on, b_on);
    }

    // EYES
    {
        uint8_t r_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_EYES][CH_R]));
        uint8_t g_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_EYES][CH_G]));
        uint8_t b_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_EYES][CH_B]));
        apply_zone(RGB_ZONE_EYES, r_on, g_on, b_on);
    }
}
