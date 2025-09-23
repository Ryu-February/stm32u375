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
		btn_id_t pressed;
		if(btn_pop_any_press(&pressed))
		{
			btn_print_one(pressed);                // ← 출력 (비파괴 아님: 이미 pop 했으니 그냥 찍기만)
			app_rgb_actions_notify_press(pressed);
		}

		app_rgb_actions_poll();
	}
}

