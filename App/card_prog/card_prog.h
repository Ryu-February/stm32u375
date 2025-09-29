/*
 * card_prog.h
 *
 *  Created on: Sep 27, 2025
 *      Author: fbcks
 */

#ifndef CARD_PROG_CARD_PROG_H_
#define CARD_PROG_CARD_PROG_H_

#include "def.h"
#include "btn.h"
#include "mode_sw.h"
#include "stepper.h"
#include "color.h"

// ===== 파라미터 튜닝 =====
#define CARD_PROG_MAX_LEN             50
#define CARD_PROG_ARM_DELAY_MS        1000  // GO 후 시작 대기
#define CARD_PROG_INTER_GAP_MS        1000  // 아이템 사이 정지 간격

// 스텝 수 매핑
#define CARD_PROG_STEPS_FORWARD       2000u
#define CARD_PROG_STEPS_BACKWARD      2000u
#define CARD_PROG_STEPS_TURN_LEFT     1150u
#define CARD_PROG_STEPS_TURN_RIGHT    1150u

typedef enum
{
    CARD_PROG_IDLE = 0,
    CARD_PROG_ARMED,
    CARD_PROG_RUNNING,
    CARD_PROG_GAP,
    CARD_PROG_PAUSED
} card_prog_state_t;

typedef enum
{
    CPOP_NONE = 0,
    CPOP_FORWARD,
    CPOP_REVERSE,
    CPOP_TURN_RIGHT,
    CPOP_TURN_LEFT
} card_prog_op_t;

// 초기화/모드
void                card_prog_init(void);
void                card_prog_set_mode(mode_sw_t m);       // MODE_CARD일 때만 활성

// 입력(좌/우 동일 색만)
void                card_prog_on_dual_equal(uint8_t left, uint8_t right);

// 버튼 제어 (GO/RESUME/DELETE만 사용)
void                card_prog_on_button(btn_id_t id);

// 서비스
void                card_prog_service(void);

// 상태/유틸
card_prog_state_t   card_prog_get_state(void);
uint8_t             card_prog_get_len(void);
uint8_t             card_prog_get_index(void);
void                card_prog_clear(void);                 // 버퍼 삭제(+STOP)
void                card_prog_stop(void);                  // 즉시 STOP(보존)
void                card_prog_start(void);                 // GO와 동일


#endif /* CARD_PROG_CARD_PROG_H_ */
