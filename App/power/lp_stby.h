/*
 * lp_stby.h
 *
 *  Created on: Sep 24, 2025
 *      Author: RCY
 */

#ifndef POWER_LP_STBY_H_
#define POWER_LP_STBY_H_

#include "def.h"


// 디바운스/홀드 시간(ms)
#ifndef LP_STBY_DEBOUNCE_MS
#define LP_STBY_DEBOUNCE_MS  30
#endif

#ifndef LP_STBY_HOLD_MS
#define LP_STBY_HOLD_MS      1000
#endif



void lp_stby_init(void);
void lp_stby_on_1ms(void);         // 1ms 주기에서 호출
void lp_stby_force(void);          // 즉시 Standby 진입

// 사용자 훅(모터 정지/LED OFF/상태 저장 등). 필요 시 오버라이드.
void lp_stby_prepare_before(void) __attribute__((weak));


#endif /* POWER_LP_STBY_H_ */
