/*
 * calib.h
 *
 *  Created on: Sep 25, 2025
 *      Author: RCY
 */

#ifndef CALIB_CALIB_H_
#define CALIB_CALIB_H_


#include "def.h"
#include "rgb.h"


typedef enum
{
    CALIB_IDLE = 0,
    CALIB_ACTIVE
} calib_state_t;

void color_calib_init(void);
void color_calib_enter(void);     // 진입 (LINE_TRACING + FORWARD 3s long)
void color_calib_exit(void);      // 강제 종료(옵션)
bool color_calib_is_active(void);

// 짧은 클릭으로 한 단계 진행(색 샘플링 & 저장)
void color_calib_on_forward_click(void);

// 필요 시 1ms 주기 업데이트(타임아웃/가이드 LED 등)
void color_calib_update_1ms(void);

// 진행률/현재 타깃 색 질의 (UI/로깅 목적)
int  color_calib_total(void);
int  color_calib_index(void);
color_t color_calib_current_target(void);


#endif /* CALIB_CALIB_H_ */
