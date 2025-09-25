/*
 * calib.c
 *
 *  Created on: Sep 25, 2025
 *      Author: RCY
 */


#include "calib.h"
#include "color.h"
#include "uart.h"


/* 내부 상태 */
static calib_state_t s_calib = CALIB_IDLE;
static int           s_calib_idx = 0;

static const color_t s_calib_order[] =
{
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN,
    COLOR_BLUE, COLOR_PURPLE, COLOR_LIGHT_GREEN, COLOR_SKY_BLUE,
    COLOR_PINK, COLOR_BLACK, COLOR_WHITE, COLOR_GRAY
};
#define CALIB_COUNT  ((int)(sizeof(s_calib_order)/sizeof(s_calib_order[0])))

/* --- 내부 구현 (calib_* 네이밍) --------------------------------------- */

void calib_enter(void)
{
    s_calib     = CALIB_ACTIVE;
    s_calib_idx = 0;
    uart_printf("[CAL] enter (total %d colors)\r\n", CALIB_COUNT);
}

void calib_exit(void)
{
    uart_printf("[CAL] done\r\n");
    s_calib = CALIB_IDLE;
}

void calib_prompt_current(void)
{
    if (s_calib != CALIB_ACTIVE)
        return;

    uart_printf("[CAL] show color %d/%d: %d\r\n",
                s_calib_idx + 1, CALIB_COUNT, (int)s_calib_order[s_calib_idx]);
}

void calib_on_forward_click(void)
{
    if (s_calib != CALIB_ACTIVE)
        return;

    color_t target = s_calib_order[s_calib_idx];

    // 한 단계 진행을 외부 태스크에 위임
//    ap_task_color_calibration();

    uart_printf("[CAL] captured color=%d\r\n", (int)target);

    s_calib_idx++;
    if (s_calib_idx >= CALIB_COUNT)
    {
        calib_exit();
    }
    else
    {
        calib_prompt_current();
    }
}

/* --- 보조 getter ------------------------------------------------------- */

bool calib_is_active(void)
{
    return (s_calib == CALIB_ACTIVE);
}

int calib_total(void)
{
    return CALIB_COUNT;
}

int calib_index(void)
{
    return s_calib_idx;
}

color_t calib_current_target(void)
{
    if (s_calib != CALIB_ACTIVE)
        return (color_t)(-1);
    return s_calib_order[s_calib_idx];
}

/* --- 선택: 1ms 주기 필요 시 ------------------------------------------- */
void calib_update_1ms(void)
{
    // 타임아웃/가이드 LED 등이 필요하면 여기 구현
}

/* --- 헤더와의 이름 불일치 해결: color_calib_* 래퍼 제공 --------------- */

void color_calib_init(void)
{
    s_calib = CALIB_IDLE;
    s_calib_idx = 0;
}

void color_calib_enter(void)
{
    calib_enter();
}

void color_calib_exit(void)
{
    calib_exit();
}

bool color_calib_is_active(void)
{
    return calib_is_active();
}

void color_calib_on_forward_click(void)
{
    calib_on_forward_click();
}

void color_calib_update_1ms(void)
{
    calib_update_1ms();
}

int color_calib_total(void)
{
    return calib_total();
}

int color_calib_index(void)
{
    return calib_index();
}

color_t color_calib_current_target(void)
{
    return calib_current_target();
}
