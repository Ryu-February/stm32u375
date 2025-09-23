/*
 * led.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "led.h"



typedef struct
{
	GPIO_TypeDef *port;
	GPIO_PinState pin;
	GPIO_PinState on_state;
	GPIO_PinState off_state;
}led_table_t;

static led_table_t led[LED_MAX_CH] =
{
	{GPIOC, GPIO_PIN_5, GPIO_PIN_RESET, GPIO_PIN_SET},
};




void led_init(void)
{
	led_on(_DEF_CH_1);
}

void led_on(uint8_t ch)
{
	if(ch >= LED_MAX_CH)
		return;

	HAL_GPIO_WritePin(led[ch].port, led[ch].pin, led[ch].on_state);
}

void led_off(uint8_t ch)
{
	if(ch >= LED_MAX_CH)
		return;

	HAL_GPIO_WritePin(led[ch].port, led[ch].pin, led[ch].on_state);
}

void led_toggle(uint8_t ch)
{
	if(ch >= LED_MAX_CH)
		return;

	HAL_GPIO_TogglePin(led[ch].port, led[ch].pin);
}
