/*
 * mode_sw.c
 *
 *  Created on: Sep 25, 2025
 *      Author: RCY
 */


#include "mode_sw.h"


typedef struct
{
	GPIO_TypeDef *port;
	uint16_t	  pin;
}mode_sw_cfg_t;

// PB3: LINE_TRACING, PB4: BUTTON, PB5: CARD (필요하면 여기만 바꾸면 됨)
static const mode_sw_cfg_t mode_cfg[MODE_COUNT - 1] =
{
    { GPIOB, GPIO_PIN_3 },   // MODE_LINE_TRACING
    { GPIOB, GPIO_PIN_4 },   // MODE_BUTTON
    { GPIOB, GPIO_PIN_5 },   // MODE_CARD
};



#ifndef MODE_SW_DEBOUNCE_MS
#define MODE_SW_DEBOUNCE_MS  8u
#endif

static uint8_t  s_raw[MODE_COUNT - 1];     // 1=LOW(선택됨), 0=HIGH
static uint8_t  s_cnt[MODE_COUNT - 1];
static uint8_t  s_stable[MODE_COUNT - 1];  // 디바운스된 값
static mode_sw_t s_cur = MODE_INVALID;
static mode_sw_t s_last_valid = MODE_BUTTON; // 전원 초기 기본 모드(원하면 변경)
static uint8_t   s_changed = 0u;


static inline uint8_t read_active_low(const mode_sw_cfg_t *p)
{
    return (HAL_GPIO_ReadPin(p->port, p->pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

static mode_sw_t decide_mode_from_lines(void)
{
    uint8_t line = s_stable[MODE_LINE_TRACING];
    uint8_t butt = s_stable[MODE_BUTTON];
    uint8_t card = s_stable[MODE_CARD];

    uint8_t sum = line + butt + card;

    if (sum != 1u)
    {
        // 불법 상태: 스위치 중복/부적절한 배선/진동
        return MODE_INVALID;
    }

    if (line) return MODE_LINE_TRACING;
    if (butt) return MODE_BUTTON;
    /*card*/  return MODE_CARD;
}

void mode_sw_init(void)
{
    for (int i = 0; i < (int)(MODE_COUNT - 1); i++)
    {
        uint8_t v = read_active_low(&mode_cfg[i]);
        s_raw[i]    = v;
        s_stable[i] = v;
        s_cnt[i]    = 0u;
    }

    s_cur = decide_mode_from_lines();
    if (s_cur != MODE_INVALID)
        s_last_valid = s_cur;
    s_changed = 0u;
}

void mode_sw_update_1ms(void)
{
    // 각 라인 디바운스
    for (int i = 0; i < (int)(MODE_COUNT - 1); i++)
    {
        uint8_t v = read_active_low(&mode_cfg[i]);
        if (v == s_stable[i])
        {
            s_cnt[i] = 0u;
            continue;
        }

        if (++s_cnt[i] >= MODE_SW_DEBOUNCE_MS)
        {
            s_stable[i] = v;
            s_cnt[i] = 0u;
        }
    }

    mode_sw_t m = decide_mode_from_lines();
    if (m == MODE_INVALID)
    {
        // 진동/불법 상태에서는 마지막 정상 모드를 유지
        m = s_last_valid;
    }
    else
    {
        s_last_valid = m;
    }

    if (m != s_cur)
    {
        s_cur = m;
        s_changed = 1u;
    }
}

mode_sw_t mode_sw_get(void)
{
    return s_cur;
}

bool mode_sw_changed(mode_sw_t *out)
{
    if (!s_changed) return false;
    s_changed = 0u;
    if (out) *out = s_cur;
    return true;
}

const char* mode_sw_name(mode_sw_t m)
{
    switch (m)
    {
        case MODE_LINE_TRACING: return "LINE_TRACING";
        case MODE_BUTTON:       return "BUTTON";
        case MODE_CARD:         return "CARD";
        default:                return "INVALID";
    }
}
