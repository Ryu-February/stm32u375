/*
 * btn_prog.h
 *
 *  Created on: Sep 26, 2025
 *      Author: RCY
 */

#ifndef BTN_PROG_BTN_PROG_H_
#define BTN_PROG_BTN_PROG_H_



#include "def.h"
#include "btn.h"
#include "mode_sw.h"
#include "stepper.h"


// ===== 설정값 =====
#define BTN_PROG_MAX_LEN             50
#define BTN_PROG_ARM_DELAY_MS        1000   // GO 후 1초 대기
#define BTN_PROG_INTER_GAP_MS        1000   // ★ 각 아이템 사이 간격(1s)


// 각 동작의 목표 스텝 수
#define BTN_PROG_STEPS_FORWARD       2000u
#define BTN_PROG_STEPS_BACKWARD      2000u
#define BTN_PROG_STEPS_TURN_LEFT     1050u
#define BTN_PROG_STEPS_TURN_RIGHT    1050u


typedef enum
{
    BTN_PROG_IDLE = 0,
    BTN_PROG_ARMED,
    BTN_PROG_RUNNING,
	BTN_PROG_GAP,
    BTN_PROG_PAUSED
} btn_prog_state_t;

// 초기화/리셋
void btn_prog_init(void);
void btn_prog_clear(void);                 // 버퍼 삭제 + 정지 (STOP)

// 입력 이벤트(버튼 모드일 때 ap에서 호출)
void btn_prog_on_button(btn_id_t id);      // 전/후/좌/우 enqueue, GO/RESUME/DELETE 처리

// 주기 서비스 (ap 메인 루프에서 반복 호출)
void btn_prog_service(mode_sw_t cur_mode, bool calib_active);

// 상태 질의(옵션용)
btn_prog_state_t btn_prog_get_state(void);
uint8_t          btn_prog_get_len(void);
uint8_t          btn_prog_get_index(void); // 0..len-1 (RUNNING일 때만 의미)



#endif /* BTN_PROG_BTN_PROG_H_ */
