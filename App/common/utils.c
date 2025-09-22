/*
 * utils.c
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */


#include "utils.h"




void delay_ms(uint32_t ms)
{
	HAL_Delay(ms);
}

uint32_t millis(void)
{
	return HAL_GetTick();
}
