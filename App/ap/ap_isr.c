/*
 * ap_isr.c
 *
 *  Created on: Sep 22, 2025
 *      Author: RCY
 */


#include "ap_isr.h"
#include "rgb.h"
#include "btn.h"
#include "rgb_actions.h"







void ap_tim4_callback(void)//10us timer
{
//	rgb_set_color(COLOR_ORANGE);
	rgb_tick();
}

void ap_tim6_callback(void)//1ms timer
{
	btn_update_1ms();

	btn_id_t pressed;
	if(btn_pop_any_press(&pressed))
	{
		app_rgb_actions_notify_press(pressed);
	}
}
