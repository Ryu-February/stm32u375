/*
 * mode_sw.h
 *
 *  Created on: Sep 25, 2025
 *      Author: RCY
 */

#ifndef INPUT_MODE_SW_H_
#define INPUT_MODE_SW_H_


#include "def.h"


typedef enum
{
    MODE_LINE_TRACING = 0,
    MODE_BUTTON,
    MODE_CARD,
    MODE_INVALID,          // 불법 상태(all HIGH/다중 LOW) 검출용
    MODE_COUNT
} mode_sw_t;

void        mode_sw_init(void);
void        mode_sw_update_1ms(void);          // 1ms 주기로 호출
mode_sw_t   mode_sw_get(void);                 // 현재 확정 모드
bool        mode_sw_changed(mode_sw_t *out);   // 변화가 있으면 true를 1회 반환(pop)
const char* mode_sw_name(mode_sw_t m);


#endif /* INPUT_MODE_SW_H_ */
