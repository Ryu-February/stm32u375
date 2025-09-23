/*
 * led.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "led.h"



typedef struct
{
	GPIO_TypeDef 	*port;
	uint16_t	 	pin;
	bool			active_low;
	bool			boot_on;   				 // 전원 인가 시 기본 ON 여부
}led_ch_cfg_t;

static const led_ch_cfg_t led_cfg[LED_CH_COUNT] =
{
	[LED_POWER_STAT] = { GPIOC, GPIO_PIN_5, true, true  },	//debug led		: low active
	[LED_W_CONTROL]  = { GPIOC, GPIO_PIN_4, false, true },	//npn-tr base	: high active
};


static bool led_state[LED_CH_COUNT] = {0};


static inline GPIO_PinState level_for(const led_ch_cfg_t *c, bool on)
{
    // on을 실제 핀 레벨로 변환
    if (c->active_low)
    {
        return on ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    else
    {
        return on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }
}


void led_init(void)
{
	for (int i = 0; i < LED_CH_COUNT; ++i)
	{
		led_state[i] = led_cfg[i].boot_on;
		HAL_GPIO_WritePin(led_cfg[i].port, led_cfg[i].pin, level_for(&led_cfg[i], led_state[i]));
	}

	led_on(LED_W_CONTROL);
	led_on(LED_POWER_STAT);
}

void led_write(led_ch_t ch, bool on)
{
    if (ch >= LED_CH_COUNT)
        return;

    led_state[ch] = on;
    HAL_GPIO_WritePin(led_cfg[ch].port, led_cfg[ch].pin, level_for(&led_cfg[ch], on));
}


void led_on(led_ch_t ch)
{
    led_write(ch, true);
}

void led_off(led_ch_t ch)
{
    led_write(ch, false);
}

void led_toggle(led_ch_t ch)
{
	bool next = !led_state[ch];
	led_state[ch] = next;

	led_write(ch, led_state[ch]);//bit 반전만.
}


bool led_is_on(led_ch_t ch)
{
    if (ch >= LED_CH_COUNT)
    {
        return false;
    }
    return led_state[ch];
}
