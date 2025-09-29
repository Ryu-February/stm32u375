/*
 * rgb_actions.h
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */

#ifndef ACTION_RGB_ACTIONS_H_
#define ACTION_RGB_ACTIONS_H_

#include "def.h"


#include "rgb.h"   // rgb_set_color(), RGB_ZONE_V_SHAPE, color_t
#include "btn.h"   // btn_id_t, BTN_* enum

void app_rgb_actions_init(void);

// 버튼 드라이버가 "눌림"을 감지했을 때만 pressed=true로 호출해줘
void app_rgb_actions_on_button_event(btn_id_t btn_id, bool pressed);

// 메인 루프에서 호출: 이벤트가 있으면 색 적용 + UART 로그 1줄
void app_rgb_actions_notify_press(btn_id_t btn_id);
void app_rgb_actions_notify_card_color(color_t color);   // <-- ADD
void app_rgb_actions_poll(uint8_t mod);

#endif /* ACTION_RGB_ACTIONS_H_ */
