/*
 * rgb_actions.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "rgb_actions.h"


static color_t color_by_button(btn_id_t id)
{
    switch (id)
    {
        case BTN_FORWARD:  return COLOR_GREEN;   // 전진 = 초록
        case BTN_BACKWARD: return COLOR_RED;     // 후진 = 빨강
        case BTN_LEFT:     return COLOR_BLUE;    // 좌회전 = 파랑
        case BTN_RIGHT:    return COLOR_YELLOW;  // 우회전 = 노랑
        case BTN_GO:       return COLOR_WHITE;   // GO = 하양
        case BTN_DELETE:   return COLOR_PINK;    // DELETE = 핑크
        case BTN_RESUME:   return COLOR_PURPLE;  // RESUME = 보라
        default:           return COLOR_BLACK;
    }
}

// ---- 이벤트 버퍼(싱글 슬롯) ----
// btn_update_1ms()가 세우고, 메인 루프에서 소비.
// enum 저장은 보통 원자적이지만, 안전하게 volatile.
static volatile int      s_evt_pending = 0;
static volatile btn_id_t s_evt_btn     = BTN_COUNT;

void app_rgb_actions_init(void)
{
    rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
    s_evt_pending = 0;
    s_evt_btn     = BTN_COUNT;
}

void app_rgb_actions_notify_press(btn_id_t btn_id)
{
#if APP_RGB_ACTIONS_ISR_APPLY
    // 초경량 경로: ISR에서 즉시 적용(로그는 안 찍음)
    rgb_set_color(RGB_ZONE_V_SHAPE, color_by_button(btn_id));
#endif
    // 이벤트를 메인 루프로 전달 (마지막 이벤트 1건만 유지)
    s_evt_btn     = btn_id;
    s_evt_pending = 1;
}


void app_rgb_actions_poll(void)
{
    if (!s_evt_pending)
    {
        return;
    }

    // 로컬로 스냅샷 떠서 경합 최소화
    btn_id_t btn = s_evt_btn;
    s_evt_pending = 0;

#if !APP_RGB_ACTIONS_ISR_APPLY
    // 색 적용은 메인 루프에서 수행 → ISR 부하 최소화
    rgb_set_color(RGB_ZONE_EYES, color_by_button(btn));
    rgb_set_color(RGB_ZONE_V_SHAPE, color_by_button(btn));
#endif

#if APP_RGB_ACTIONS_ENABLE_LOG
    uart_printf("[RGB] V-SHAPE <- btn=%d\r\n", (int)btn);
#endif
}
