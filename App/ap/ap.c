/*
 * ap.c
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */



#include "ap.h"


extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim6;


void ap_init(void)
{
//	uart_init();

	led_init();
	rgb_init();

	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_Base_Start_IT(&htim6);
}

void ap_main(void)
{
	while(1)
	{
//		rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_YELLOW);
		btn_print_events();
	}
}

