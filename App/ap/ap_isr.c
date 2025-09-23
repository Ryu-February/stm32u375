/*
 * ap_isr.c
 *
 *  Created on: Sep 22, 2025
 *      Author: RCY
 */


#include "ap_isr.h"
#include "rgb.h"







void ap_tim4_callback(void)
{
//	rgb_set_color(COLOR_ORANGE);
	rgb_tick();
}

void ap_tim6_callback(void)
{

}
