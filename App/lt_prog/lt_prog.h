/*
 * lt_prog.h
 *
 *  Created on: Sep 29, 2025
 *      Author: RCY
 */

#ifndef LT_PROG_LT_PROG_H_
#define LT_PROG_LT_PROG_H_

#include "def.h"
#include "line_tracing.h"
#include "btn.h"

typedef enum
{
    LT_PROG_IDLE = 0,
    LT_PROG_RUNNING
} lt_prog_state_t;

/*
 * 정책(요구사항):
 * - EXECUTE  : 라인트레이싱 실행(시작)
 * - RESUME   : 라인트레이싱 정지(토글의 '정지' 역할)
 * - DELETE   : 아무 일도 하지 않음(무시)
 *
 * 주기 업데이트는 lt_prog_service(now_ms)에서 line_tracing_update(now_ms)를 위임.
 */

void lt_prog_init(const lt_config_t *cfg);
void lt_prog_reset(void);  // 강제 초기화(Idle + LT 비활성)

void lt_prog_on_action_execute(void);
void lt_prog_on_action_resume(void);
void lt_prog_on_action_delete(void);

void lt_prog_on_button(btn_id_t id);   // 프로젝트 버튼에 매핑하고 싶을 때만 사용

void lt_prog_service(uint32_t now_ms);

bool lt_prog_is_running(void);
lt_prog_state_t lt_prog_get_state(void);

#endif /* LT_PROG_LT_PROG_H_ */
