/*
 * lt_prog.c
 *
 *  Created on: Sep 29, 2025
 *      Author: RCY
 */


#include "lt_prog.h"
#include "uart.h"       // 디버깅 로그 용(옵션)

#ifndef LT_PROG_LOG_ENABLED
#define LT_PROG_LOG_ENABLED   1
#endif

#if LT_PROG_LOG_ENABLED
#define LT_LOG(fmt, ...)  uart_printf("[LT_PROG] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LT_LOG(fmt, ...)
#endif

static lt_prog_state_t s_state = LT_PROG_IDLE;
static lt_config_t     s_cfg;

static inline void lt_start(void)
{
    if (s_state == LT_PROG_RUNNING)
    {
        return;
    }

    line_tracing_init(&s_cfg);
    line_tracing_enable(true);
    s_state = LT_PROG_RUNNING;

    LT_LOG("start: Kp=%.2f Ki=%.2f Kd=%.2f base=%u min=%u max=%u dt=%ums",
           s_cfg.Kp, s_cfg.Ki, s_cfg.Kd,
           s_cfg.base_ticks, s_cfg.min_ticks, s_cfg.max_ticks, s_cfg.interval_ms);
}

static inline void lt_stop(void)
{
    if (s_state == LT_PROG_IDLE)
    {
        return;
    }

    line_tracing_enable(false);
    s_state = LT_PROG_IDLE;

    LT_LOG("stop");
}

void lt_prog_init(const lt_config_t *cfg)
{
    if (cfg)
    {
        s_cfg = *cfg;
    }
    else
    {
        // 합리적 기본값
        s_cfg.Kp          = 0.5f;
        s_cfg.Ki          = 0.0f;
        s_cfg.Kd          = 0.2f;
        s_cfg.base_ticks  = 1500;
        s_cfg.min_ticks   = 500;
        s_cfg.max_ticks   = 2500;
        s_cfg.interval_ms = 5;
    }

    s_state = LT_PROG_IDLE;
    line_tracing_enable(false);

    LT_LOG("init");
}

void lt_prog_reset(void)
{
    lt_stop();
    line_tracing_init(&s_cfg);
    LT_LOG("reset");
}

/* 정책 그대로 구현:
 * - EXECUTE  : 실행(시작)
 * - RESUME   : 멈춤
 * - DELETE   : 무시
 */
void lt_prog_on_action_execute(void)
{
    lt_start();
}

void lt_prog_on_action_resume(void)
{
    lt_stop();   // 요구사항: resume이면 '멈춤' 동작
}

void lt_prog_on_action_delete(void)
{
    // Do nothing
    LT_LOG("delete: ignored");
}

/* 프로젝트 버튼 매핑(원하면 사용) */
void lt_prog_on_button(btn_id_t id)
{
    switch (id)
    {
        case BTN_EXECUTE:
            lt_prog_on_action_execute();
            break;
        case BTN_RESUME:
            lt_prog_on_action_resume();
            break;

        case BTN_DELETE:
            lt_prog_on_action_delete();
            break;

        default:
            // 라인트레이싱에서는 다른 버튼은 무시
            break;
    }
}

/* 주기적 서비스: 라인트레이싱이 Running일 때만 업데이트 위임 */
void lt_prog_service(uint32_t now_ms)
{
    if (s_state == LT_PROG_RUNNING)
    {
        line_tracing_update(now_ms);
    }
}

bool lt_prog_is_running(void)
{
    return (s_state == LT_PROG_RUNNING);
}

lt_prog_state_t lt_prog_get_state(void)
{
    return s_state;
}
