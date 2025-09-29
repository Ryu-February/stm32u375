/*
 * utils.c
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */


#include "utils.h"


extern volatile uint32_t timer6_ms;


void delay_ms(uint32_t ms)
{
	HAL_Delay(ms);
}

uint32_t millis(void)
{
	return HAL_GetTick();
}


uint32_t tim6_get_ms(void)
{
	return timer6_ms;
}
