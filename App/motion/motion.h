/*
 * motion.h
 *
 *  Created on: Sep 17, 2025
 *      Author: RCY
 */

#ifndef MOTION_MOTION_H_
#define MOTION_MOTION_H_

#include "def.h"
#include "rgb.h"


void    motion_init(void);
void    motion_plan_color(color_t c);   // 색 감지 시 1번 호출
void    motion_service(void);           // 1ms 등 폴링에서 호출
bool    motion_is_running(void);
uint32_t get_goal_steps(void);


#endif /* MOTION_MOTION_H_ */
