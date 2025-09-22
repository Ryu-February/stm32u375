/*
 * ap_isr.c
 *
 *  Created on: Sep 22, 2025
 *      Author: RCY
 */


#include "ap_isr.h"





void ap_tim4_callback(void)
{

}

void ap_tim6_callback(void)
{
	static uint8_t tick = 0;

	if(++tick >= 1000)
	{
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_5);
		tick = 0;
	}

}
