/*
 * lp_stby.c
 *
 *  Created on: Sep 24, 2025
 *      Author: RCY
 */


#include "lp_stby.h"

// ==== 하드웨어 핀 ====
// PB1에 Delete 스위치 (프로젝트 기준)
#define LP_STBY_BTN_PORT     GPIOB
#define LP_STBY_BTN_PIN      GPIO_PIN_1

// 내부 상태
static uint8_t  s_last_raw      = 1;   // 마지막 샘플(raw)
static uint16_t s_stable_ms     = 0;   // 같은 raw가 지속된 ms
static uint8_t  s_stable_state  = 1;   // 디바운스 통과 상태(1: not pressed)
static uint32_t s_press_ms      = 0;   // 눌림 지속 ms
static bool     s_fired         = false;

static inline bool lp_stby_read_pressed(void)
{
    GPIO_PinState pin = HAL_GPIO_ReadPin(LP_STBY_BTN_PORT, LP_STBY_BTN_PIN);

#if LP_STBY_ACTIVE_LOW
    return (pin == GPIO_PIN_RESET);   // 활성 Low
#else
    return (pin == GPIO_PIN_SET);
#endif
}

void lp_stby_init(void)
{
    bool pressed = lp_stby_read_pressed();

    s_last_raw      = pressed ? 0 : 1;  // 0: pressed(활성저레벨 가정), 1: not pressed
    s_stable_state  = s_last_raw;
    s_stable_ms     = 0;
    s_press_ms      = 0;
    s_fired         = false;
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
    // 1) 디바운스
    uint8_t raw = lp_stby_read_pressed() ? 0 : 1; // 0: pressed, 1: not pressed

    if (raw == s_last_raw)
    {
        if (s_stable_ms < 0xFFFF)
        {
            s_stable_ms++;
        }
    }
    else
    {
        s_stable_ms = 0;
        s_last_raw  = raw;
    }

    if (s_stable_ms == LP_STBY_DEBOUNCE_MS)
    {
        s_stable_state = raw;
    }

    // 2) 홀드 체크
#if LP_STBY_ACTIVE_LOW
    bool pressed = (s_stable_state == 0);
#else
    bool pressed = (s_stable_state == 1);
#endif

    if (pressed && !s_fired)
    {
        if (s_press_ms < 0x7FFFFFFF)
        {
            s_press_ms++;
        }

        if (s_press_ms >= LP_STBY_HOLD_MS)
        {
            s_fired = true;
            lp_stby_enter();
        }
    }
    else if (!pressed)
    {
        s_press_ms = 0;  // 손 뗐으면 초기화
    }
}
