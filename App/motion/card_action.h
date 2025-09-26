/*
 * card_action.h
 *
 *  Created on: Sep 27, 2025
 *      Author: fbcks
 */

#ifndef MOTION_CARD_ACTION_H_
#define MOTION_CARD_ACTION_H_

#include "def.h"

#include "btn.h"
#include "color.h"
#include "mode_sw.h"



void     card_action_init(void);
void     card_action_set_mode(mode_sw_t m);         // MODE_CARD일 때만 활성
void     card_action_stop(void);                    // 즉시 정지(버퍼 없음, 단발)

// 좌/우 컬러가 같을 때만 내부적으로 plan(trigger)
void     card_action_on_dual_equal(uint8_t left, uint8_t right);

// 직접 색 1개로 단발 실행을 걸고 싶을 때 사용(좌우 동일 체크 생략 버전)
void     card_action_plan(color_t c);

void     card_action_service(void);                 // 폴링(1ms 등)
bool     card_action_is_running(void);
uint32_t card_action_get_goal_steps(void);


#endif /* MOTION_CARD_ACTION_H_ */
