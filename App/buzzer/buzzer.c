/*
 * buzzer.c
 *
 *  Created on: Sep 29, 2025
 *      Author: RCY
 */

#include "buzzer.h"

// CubeMX가 만든 핸들 외부참조
extern TIM_HandleTypeDef BUZZER_TIM;

// ===== 내부 상태 =====
typedef struct
{
    uint32_t hz;
    uint16_t on_ms;
    uint16_t off_ms;
    uint8_t  repeat;      // 남은 반복
    uint8_t  duty_pct;    // 0~100
} buz_cmd_t;

typedef enum
{
    BZ_IDLE = 0,
    BZ_ON,
    BZ_OFF
} bz_state_t;

#define BZ_QUEUE_LEN   8

static volatile bz_state_t s_state = BZ_IDLE;
static volatile uint16_t   s_left_ms = 0;
static volatile buz_cmd_t  s_cur = {0};
static buz_cmd_t           s_q[BZ_QUEUE_LEN];
static volatile uint8_t    s_q_head = 0, s_q_tail = 0;

// 연속음 모드(패턴과 별개): true면 항상 PWM 유지
static volatile bool       s_tone_hold = false;
static volatile uint32_t   s_tone_hold_hz = 0;
static volatile uint8_t    s_tone_hold_duty = 50;

// ===== 유틸 =====
static inline bool q_empty(void)
{
    return s_q_head == s_q_tail;
}
static inline bool q_full(void)
{
    return (uint8_t)((s_q_head + 1U) % BZ_QUEUE_LEN) == s_q_tail;
}
static bool q_push(buz_cmd_t c)
{
    if (q_full()) return false;
    s_q[s_q_head] = c;
    s_q_head = (uint8_t)((s_q_head + 1U) % BZ_QUEUE_LEN);
    return true;
}
static bool q_pop(buz_cmd_t *out)
{
    if (q_empty()) return false;
    *out = s_q[s_q_tail];
    s_q_tail = (uint8_t)((s_q_tail + 1U) % BZ_QUEUE_LEN);
    return true;
}

static uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ARR/CCR 계산 및 적용
static void pwm_apply(uint32_t hz, uint8_t duty_pct)
{
    hz = clamp_u32(hz, BUZZER_MIN_HZ, BUZZER_MAX_HZ);
    duty_pct = clamp_u8(duty_pct, 1, 99); // 0/100은 완전저/완전고정 → 피함

    // f_tim = 1MHz → ARR = f_tim / f - 1
    uint32_t arr = (BUZZER_TIMER_CLK_HZ / hz);
    if (arr == 0) arr = 1;
    arr -= 1U;
    if (arr > 0xFFFFU) arr = 0xFFFFU;

    uint32_t period = arr + 1U;
    uint32_t ccr = (period * duty_pct) / 100U;
    if (ccr == 0) ccr = 1;                 // 최소 펄스 보장
    if (ccr >= period) ccr = period - 1U;  // 최대 펄스 제한

    // 안전하게 채널 정지 후 갱신
    HAL_TIM_PWM_Stop(&BUZZER_TIM, BUZZER_TIM_CHANNEL);

    __HAL_TIM_SET_AUTORELOAD(&BUZZER_TIM, (uint32_t)arr);
    __HAL_TIM_SET_COMPARE(&BUZZER_TIM, BUZZER_TIM_CHANNEL, (uint32_t)ccr);

    // 갱신 이벤트 발생(ARR 버퍼 적용)
    HAL_TIM_GenerateEvent(&BUZZER_TIM, TIM_EVENTSOURCE_UPDATE);

    HAL_TIM_PWM_Start(&BUZZER_TIM, BUZZER_TIM_CHANNEL);
}

static void pwm_stop(void)
{
    HAL_TIM_PWM_Stop(&BUZZER_TIM, BUZZER_TIM_CHANNEL);
}

// ===== 퍼블릭 구현 =====
void buzzer_init_pwm(void)
{
    // CubeMX에서 TIM16 초기화 끝났다고 가정
    s_state = BZ_IDLE;
    s_left_ms = 0;
    s_q_head = s_q_tail = 0;
    s_tone_hold = false;
    s_tone_hold_hz = 0;
    s_tone_hold_duty = 50;
    pwm_stop();
}

void buzzer_stop(void)
{
    // 패턴/연속음 모두 정지
    s_state = BZ_IDLE;
    s_left_ms = 0;
    s_q_head = s_q_tail = 0;
    s_tone_hold = false;
    pwm_stop();
}

bool buzzer_is_busy(void)
{
    return s_tone_hold || (s_state != BZ_IDLE) || !q_empty();
}

uint8_t buzzer_queue_free(void)
{
    uint8_t used = (s_q_head >= s_q_tail)
                 ? (s_q_head - s_q_tail)
                 : (uint8_t)(BZ_QUEUE_LEN - (s_q_tail - s_q_head));
    return (uint8_t)(BZ_QUEUE_LEN - 1U - used);
}

bool buzzer_tone_start(uint32_t hz, uint8_t duty_pct)
{
    pwm_apply(hz, duty_pct);
    s_tone_hold = true;
    s_tone_hold_hz = hz;
    s_tone_hold_duty = duty_pct;
    return true;
}

void buzzer_tone_stop(void)
{
    s_tone_hold = false;
    s_tone_hold_hz = 0;
    pwm_stop();
}

bool buzzer_tone_once(uint32_t hz, uint16_t on_ms, uint8_t duty_pct)
{
    if (on_ms == 0) return true;
    buz_cmd_t c = (buz_cmd_t){ .hz = hz, .on_ms = on_ms, .off_ms = 0, .repeat = 1, .duty_pct = duty_pct };
    return q_push(c);
}

bool buzzer_tone_pattern(uint32_t hz, uint16_t on_ms, uint16_t off_ms,
                         uint8_t repeat, uint8_t duty_pct)
{
    if (repeat == 0) return true;
    buz_cmd_t c = (buz_cmd_t){ .hz = hz, .on_ms = on_ms, .off_ms = off_ms, .repeat = repeat, .duty_pct = duty_pct };
    return q_push(c);
}

void buzzer_update_1ms(void)
{
    // 연속음 모드면 패턴 무시하고 PWM 유지
    if (s_tone_hold)
    {
        // 혹시 외부에서 TIM이 멈췄다면 재시작 보정(옵션)
        // HAL_TIM_PWM_StateTypeDef 등으로 점검 가능하지만 비용 아껴 패스
        return;
    }

    switch (s_state)
    {
        case BZ_IDLE:
        {
            if (!q_pop((buz_cmd_t *)&s_cur))
            {
                // 대기
                pwm_stop();
                return;
            }
            // ON 구간 시작
            pwm_apply(s_cur.hz, s_cur.duty_pct);
            s_left_ms = s_cur.on_ms;
            s_state = BZ_ON;
            break;
        }

        case BZ_ON:
        {
            if (s_left_ms > 0) s_left_ms--;
            if (s_left_ms == 0)
            {
                // OFF 구간으로
                pwm_stop();
                s_left_ms = s_cur.off_ms;
                s_state = BZ_OFF;

                // 바로 쉬는 시간이 0이면 반복 카운트 처리로 진행
                if (s_left_ms == 0)
                {
                    if (s_cur.repeat > 0) s_cur.repeat--;
                    if (s_cur.repeat > 0)
                    {
                        // 다음 사이클 ON
                        pwm_apply(s_cur.hz, s_cur.duty_pct);
                        s_left_ms = s_cur.on_ms;
                        s_state = BZ_ON;
                    }
                    else
                    {
                        s_state = BZ_IDLE;
                    }
                }
            }
            break;
        }

        case BZ_OFF:
        {
            if (s_left_ms > 0) s_left_ms--;
            if (s_left_ms == 0)
            {
                if (s_cur.repeat > 0) s_cur.repeat--;
                if (s_cur.repeat > 0)
                {
                    // 다음 사이클 ON
                    pwm_apply(s_cur.hz, s_cur.duty_pct);
                    s_left_ms = s_cur.on_ms;
                    s_state = BZ_ON;
                }
                else
                {
                    s_state = BZ_IDLE;
                }
            }
            break;
        }
    }
}

void buzzer_play_pororororong(void)
{
    // 포르테: 짧게 위로 스텝업하면서 “또르르르” 느낌 + 긴 테일
    // 각 스텝: on=60ms, off=20ms (마지막 스텝만 off=0)
    // 전체 길이 ≈ 7*(60+20) + 300 = 860 ms
    // 큐 사용: 8개 (7스텝 + 테일 1개) → BZ_QUEUE_LEN=8에 맞춤

    (void)buzzer_tone_pattern( 900,  60, 20, 1, 50);  // poro-
    (void)buzzer_tone_pattern(1050,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1250,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1450,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1700,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1950,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(2200,  60,  0, 1, 50);  // 마지막 또르르 끝

    // 롱— (조금 낮춰서 1.1kHz로 300ms sustain)
    (void)buzzer_tone_pattern(1100, 300,  0, 1, 50);
}

void buzzer_play_shutdown_pororororong(void)
{
    // 먼저 다른 소리(루프 포함) 전부 정리
    buzzer_stop();

    // 상승 버전과 동일한 길이/리듬을 그대로 뒤집음
    // 각 스텝: on=60ms, off=20ms (마지막 스텝만 off=0)
    // 총 큐 사용 8개(7스텝 + 테일 1개) → 기본 BZ_QUEUE_LEN=8에 맞춤

    (void)buzzer_tone_pattern(2200,  60, 20, 1, 50);  // 롱의 시작을 높은 톤에서
    (void)buzzer_tone_pattern(1950,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1700,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1450,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1250,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1050,  60, 20, 1, 50);
    (void)buzzer_tone_pattern( 900,  60,  0, 1, 50);  // 또르르 마무리

    // 마지막 “롱—”은 더 낮게 700Hz로 300ms 유지 (살짝 잔향 느낌)
    (void)buzzer_tone_pattern( 700, 300,  0, 1, 45);
}

void buzzer_play_resume(void)
{
    // 딩-딩-딩↗ + 살짝 길게 끝: 총 ~520ms
    // on/off는 짧고 경쾌, 주파수는 점점 상승
    (void)buzzer_tone_pattern( 900,  80, 30, 1, 50);
    (void)buzzer_tone_pattern(1200,  80, 30, 1, 50);
    (void)buzzer_tone_pattern(1500, 100, 40, 1, 50);
    (void)buzzer_tone_pattern(1800, 140,  0, 1, 50);  // 마무리 롱
}

void buzzer_play_execute(void)
{
    // 타-타-타아— : 0.07s, 0.07s, 0.30s (사이 40ms 갭)
    // 총 ~520ms, 상승(0.9→1.2→1.6kHz), duty 55%
    (void)buzzer_tone_pattern( 900,  70, 40, 1, 55);
    (void)buzzer_tone_pattern(1200,  70, 40, 1, 55);
    (void)buzzer_tone_pattern(1600, 300,  0, 1, 55);
}


void buzzer_play_birik(void)
{
    // “삐↗릭” : 짧게 올렸다가 살짝 내려오며 마무리 (~165 ms)
    // 큐 사용 3칸
    (void)buzzer_tone_pattern(1400, 40, 10, 1, 55);
    (void)buzzer_tone_pattern(1900, 55,  0, 1, 55);
    (void)buzzer_tone_pattern(1100, 70,  0, 1, 50);
}

void buzzer_play_biriririring(void)
{
    // 큐 여유가 부족하면 간단 버전으로 대체
    if (buzzer_queue_free() < 7)
    {
        // 최소 보장: 1.4 kHz 250 ms
        (void)buzzer_tone_once(1400, 250, 50);
        return;
    }

    // “삐리리리링”: 빠른 트릴(주파수 교차) 6스텝 + 롱 테일
    // 각 스텝: on=40ms, off=10ms, duty=50~55%
    // 총 길이 ≈ 6*(40+10) + 300 = 600 ms
    (void)buzzer_tone_pattern(1300, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1600, 40, 10, 1, 55);
    (void)buzzer_tone_pattern(1350, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1650, 40, 10, 1, 55);
    (void)buzzer_tone_pattern(1400, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1700, 40,  0, 1, 55); // 트릴 끝

    // 링— 테일
    (void)buzzer_tone_pattern(1400, 300, 0, 1, 50);
}

void buzzer_play_no_index(void)
{
    // “뚝-뚝↓” : 하강 두음 → ‘없음/불가’ 직관적
    // 800 Hz 120ms, 60ms 쉼 → 600 Hz 220ms
    (void)buzzer_tone_pattern( 800, 120, 60, 1, 45);
    (void)buzzer_tone_pattern( 600, 220,  0, 1, 45);
}

// ↑: 짧은 상승 두음 (상향 느낌)
void buzzer_play_input_up(void)
{
    (void)buzzer_tone_pattern(1200, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1500, 60,  0, 1, 50);
}

// ↓: 짧은 하강 두음 (하향 느낌)
void buzzer_play_input_down(void)
{
    (void)buzzer_tone_pattern(1500, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1200, 60,  0, 1, 50);
}

// ←: 저음 단음(좌, 낮은 피치)
void buzzer_play_input_left(void)
{
    (void)buzzer_tone_pattern( 900, 70, 0, 1, 45);
}

// →: 고음 단음(우, 높은 피치)
void buzzer_play_input_right(void)
{
    (void)buzzer_tone_pattern(1800, 70, 0, 1, 45);
}
