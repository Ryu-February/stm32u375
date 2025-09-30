/*
 * rgb_actions.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "rgb_actions.h"
#include "mode_sw.h"
#include "card_prog.h"
#include "btn_prog.h"


static color_t color_by_button(btn_id_t id)
{
    switch (id)
    {
        case BTN_FORWARD:  return COLOR_GREEN;   // 전진 = 초록
        case BTN_BACKWARD: return COLOR_RED;     // 후진 = 빨강
        case BTN_LEFT:     return COLOR_BLUE;    // 좌회전 = 파랑
        case BTN_RIGHT:    return COLOR_YELLOW;  // 우회전 = 노랑
        case BTN_EXECUTE:  return COLOR_WHITE;   // GO = 하양
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
// ---- ★ 카드 색상 이벤트(싱글 슬롯) 추가 ----
static volatile int      s_evt_color_pending = 0;     // <-- ADD
static volatile color_t  s_evt_color          = COLOR_BLACK; // <-- ADD

void app_rgb_actions_init(void)
{
    rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
    rgb_set_color(RGB_ZONE_EYES, COLOR_BLACK);
    s_evt_pending		 = 0;
    s_evt_btn    		 = BTN_COUNT;
    s_evt_color_pending  = 0;              // <-- ADD
    s_evt_color          = COLOR_BLACK;    // <-- ADD
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

// ---- ★ 카드 색상 이벤트 통지 API ----
void app_rgb_actions_notify_card_color(color_t color)     // <-- ADD
{
    s_evt_color         = color;
    s_evt_color_pending = 1;
}



void app_rgb_actions_poll(uint8_t mod)
{
#if !APP_RGB_ACTIONS_ISR_APPLY
	// ---- 버튼 이벤트 처리(기존 로직) ----
	if (!s_evt_pending)
	{
		return;
	}

	btn_id_t btn = s_evt_btn;
	s_evt_pending = 0;

	if (mod == MODE_LINE_TRACING || mod == MODE_BUTTON)
	{
		if(btn_prog_get_state() == (BTN_PROG_RUNNING | BTN_PROG_GAP | BTN_PROG_ARMED))
			return;

		color_t c = color_by_button(btn);
		if(c == COLOR_WHITE)
			return;
		rgb_set_color(RGB_ZONE_EYES,    c);
		rgb_set_color(RGB_ZONE_V_SHAPE, c);
	}
#endif

   // ----- * 카드 프로그래밍 동작 중엔 색상 입력 안 받게 처리
	if(card_prog_get_state() == CARD_PROG_RUNNING)
		return;
	if(card_prog_get_state() == CARD_PROG_GAP)
		return;

	// ---- ★ 카드 색상 이벤트가 있으면 우선 처리 ----
	if (s_evt_color_pending)                               // <-- ADD
	{
		color_t col = s_evt_color;
		s_evt_color_pending = 0;

		rgb_set_color(RGB_ZONE_EYES,    col);
		rgb_set_color(RGB_ZONE_V_SHAPE, col);

#if APP_RGB_ACTIONS_ENABLE_LOG
		uart_printf("[RGB] CARD color=%d\r\n", (int)col);
#endif
		return; // ★ 카드 이벤트를 소비했으면 여기서 종료
	}
#if APP_RGB_ACTIONS_ENABLE_LOG
	uart_printf("[RGB] V-SHAPE <- btn=%d\r\n", (int)btn);
#endif
}
