/*
 * ap_isr.c
 *
 *  Created on: Sep 22, 2025
 *      Author: RCY
 */


#include "ap_isr.h"
#include "rgb.h"
#include "btn.h"
#include "stepper.h"
#include "lp_stby.h"
#include "mode_sw.h"
#include "buzzer.h"

volatile uint32_t timer6_ms;




void ap_tim2_callback(void)
{

}


void ap_tim4_callback(void)//10us timer
{
//	rgb_set_color(COLOR_ORANGE);
	rgb_tick();

	step_tick_isr();
}

void ap_tim6_callback(void)//1ms timer
{
	timer6_ms++;

	btn_update_1ms();
	lp_stby_on_1ms();
	mode_sw_update_1ms();
	buzzer_update_1ms();

}
