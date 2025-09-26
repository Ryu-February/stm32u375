/*
 * lp_stby.c
 *
 *  Created on: Sep 24, 2025
 *      Author: RCY
 */


#include "lp_stby.h"

// ==== 하드웨어 핀 ====
// PB1에 Delete 스위치 (프로젝트 기준)
#define LP_STBY_BTN_PORT     GPIOA
#define LP_STBY_BTN_PIN      GPIO_PIN_0

// 내부 상태
static bool		s_last_pressed   = false;	// 마지막 샘플(raw)
static uint16_t s_stable_ms      = 0;   		// 같은 raw가 지속된 ms
static bool 	s_stable_pressed = false;   	// 디바운스 통과 상태(1: not pressed)
static uint32_t s_press_ms       = 0;   		// 눌림 지속 ms
static bool     s_fired          = false;

static inline bool btn_pressed_raw(void)
{
    // Active-Low: 눌림 → LOW
    return (HAL_GPIO_ReadPin(LP_STBY_BTN_PORT, LP_STBY_BTN_PIN) == GPIO_PIN_RESET);
}

void lp_stby_init(void)
{
    bool pressed = btn_pressed_raw();

    s_last_pressed   = pressed;		  // 0: pressed(활성저레벨 가정), 1: not pressed
    s_stable_pressed = pressed;
    s_stable_ms      = 0;
    s_press_ms       = 0;
    s_fired          = false;
}

void lp_stby_prepare_before(void)
{
    // 사용자가 오버라이드:
    // - 모터/버저/LED OFF
    // - 상태 저장(Flash/EEPROM)
    // - 주변장치 안전 종료
}

static void lp_stby_enter(void)
{
    lp_stby_prepare_before();

    __disable_irq();
    HAL_SuspendTick();

    // Wake-up 소스는 CubeMX에서 설정(예: RTC Alarm, WKUP 핀)
    HAL_PWR_EnterSTANDBYMode();

//    while (1)
//    {
//        // 돌아오지 않음
//    }
}

void lp_stby_force(void)
{
    s_fired = true;
    lp_stby_enter();
}

void lp_stby_on_1ms(void)
{
    bool raw_pressed = btn_pressed_raw();

    if (raw_pressed == s_last_pressed)
    {
        if (s_stable_ms < 0xFFFF) s_stable_ms++;
    }
    else
    {
        s_stable_ms    = 0;
        s_last_pressed = raw_pressed;
    }

    if (s_stable_ms == LP_STBY_DEBOUNCE_MS)
    {
        s_stable_pressed = raw_pressed;
    }

    if (s_stable_pressed && !s_fired)
    {
        if (s_press_ms < 0x7FFFFFFF) s_press_ms++;
        if (s_press_ms >= LP_STBY_HOLD_MS)
        {
            s_fired = true;
            lp_stby_enter();
        }
    }
    else if (!s_stable_pressed)
    {
        s_press_ms = 0;
    }
}
