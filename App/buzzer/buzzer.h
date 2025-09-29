/*
 * buzzer.h
 *
 *  Created on: Sep 29, 2025
 *      Author: RCY
 */

#ifndef BUZZER_BUZZER_H_
#define BUZZER_BUZZER_H_

#include "def.h"

// ===== 하드웨어/타이머 파라미터 =====
// TIM16은 CubeMX에서 아래와 같이 설정되어 있다고 가정:
// - CH1: PA6 (AF 설정)
// - PSC: 95  → TIM16CLK / (PSC+1) = 96MHz / 96 = 1MHz
// - CNT 모드: Up, Preload enable 권장(ARPE)
#ifndef BUZZER_TIM
#define BUZZER_TIM           htim16
#endif

#ifndef BUZZER_TIM_CHANNEL
#define BUZZER_TIM_CHANNEL   TIM_CHANNEL_1
#endif

#ifndef BUZZER_TIMER_CLK_HZ
#define BUZZER_TIMER_CLK_HZ  (1000000UL) // PSC=95 기준 1MHz
#endif

// 안전 주파수 범위(패시브 부저 권장)
#ifndef BUZZER_MIN_HZ
#define BUZZER_MIN_HZ        (100U)
#endif
#ifndef BUZZER_MAX_HZ
#define BUZZER_MAX_HZ        (10000U)
#endif

// ===== 퍼블릭 API =====
void buzzer_init_pwm(void);                                  // PWM 초기화(정지)
void buzzer_stop(void);                                      // 강제 정지(큐 무시 X)
bool buzzer_is_busy(void);
uint8_t buzzer_queue_free(void);

// 즉시 연속음 시작/정지
bool buzzer_tone_start(uint32_t hz, uint8_t duty_pct);       // 무한 재생(패턴과 독립)
void buzzer_tone_stop(void);

// 단발/패턴(비차단 큐)
bool buzzer_tone_once(uint32_t hz, uint16_t on_ms, uint8_t duty_pct);
bool buzzer_tone_pattern(uint32_t hz, uint16_t on_ms, uint16_t off_ms,
                         uint8_t repeat, uint8_t duty_pct);

// 1ms 주기에서 호출(TIM6 등)
void buzzer_update_1ms(void);

// ===== 편의 매크로(요청했던 이벤트 예시) =====
static inline void buzzer_evt_delete(void)   // 0.3초 × 3회 @ 2kHz
{
    buzzer_tone_pattern(2000, 300, 300, 3, 50);
}
static inline void buzzer_evt_resume(void)   // 지속음 @ 1.5kHz
{
    buzzer_tone_start(1500, 50);
}
static inline void buzzer_evt_execute(void)  // 0.5초 1회 @ 1kHz
{
    buzzer_tone_once(1000, 500, 50);
}


#endif /* BUZZER_BUZZER_H_ */
