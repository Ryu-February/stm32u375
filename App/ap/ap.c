/*
 * ap.c
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */



#include "ap.h"


extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim6;




void ap_init(void)
{
	uart_init();

	led_init();
	rgb_init();

	step_init_all();
	lp_stby_init();

	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_Base_Start_IT(&htim6);
}



void ap_main(void)
{
	while(1)
	{
		btn_id_t pressed;
		StepOperation op;

		if(btn_pop_any_press(&pressed))
		{
			btn_print_one(pressed);                // ← 출력 (비파괴 아님: 이미 pop 했으니 그냥 찍기만)
			app_rgb_actions_notify_press(pressed);
			app_rgb_actions_poll();

			switch (pressed)
			{
				case BTN_GO:
					op = OP_STOP;
					break;
				case BTN_FORWARD:
					op = OP_FORWARD;
					break;
				case BTN_BACKWARD:
					op = OP_REVERSE;
					break;
				case BTN_LEFT:
					op = OP_TURN_LEFT;
					break;
				case BTN_RIGHT:
					op = OP_TURN_RIGHT;
					break;
				default:
					break;

			}
		}
		step_drive(op);
	}
}

