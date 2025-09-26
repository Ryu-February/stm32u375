/*
 * btn_action.h
 *
 *  Created on: Sep 26, 2025
 *      Author: RCY
 */

#ifndef MOTION_BTN_ACTION_H_
#define MOTION_BTN_ACTION_H_

#include "def.h"
#include "btn.h"

void     btn_action_init(void);
void     btn_action_plan(btn_id_t b);   // 버튼 1회 눌림 → 그 동작 1회 실행
void     btn_action_service(void);      // 폴링(1ms 등)
void     btn_action_stop(void);         // 즉시 정지
bool     btn_action_is_running(void);
uint32_t btn_action_get_goal_steps(void);

#endif /* MOTION_BTN_ACTION_H_ */
