/*
 * led.h
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */

#ifndef LED_LED_H_
#define LED_LED_H_


#include "def.h"


void led_init(void);

void led_on(uint8_t ch);
void led_off(uint8_t ch);
void led_toggle(uint8_t ch);



#endif /* LED_LED_H_ */
