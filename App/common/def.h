/*
 * def.h
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */

#ifndef COMMON_DEF_H_
#define COMMON_DEF_H_



#include <stdio.h>

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <stdbool.h>


#include "main.h"


#define _DEF_CH_1	0
#define _DEF_CH_2	1
#define _DEF_CH_3	2
#define _DEF_CH_4	3
#define _DEF_CH_5	4
#define _DEF_CH_6	5
#define _DEF_CH_7	6

#define LED_MAX_CH	3



#define LEFT        0
#define RIGHT       1



void delay_ms(uint32_t ms);
uint32_t millis(void);



#endif /* COMMON_DEF_H_ */
