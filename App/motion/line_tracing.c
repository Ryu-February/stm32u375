/*
 * line_tracing.c
 *
 *  Created on: Sep 17, 2025
 *      Author: RCY
 */


#include <stdlib.h>
#include "line_tracing.h"

// 프로젝트 환경에 맞게 필요한 헤더로 교체/추가하세요.
#include "color.h"     // bh1749_read_rgbc, BH1749_ADDR_LEFT/RIGHT
#include "stepper.h"    // step_drive, step_drive_ratio, OP_*
#include "uart.h"       // (옵션) 디버깅 출력

static lt_config_t g_cfg;
static bool        g_enabled = false;

static float   prev_error = 0.0f;
static float   integral   = 0.0f;
static uint32_t prev_ms   = 0;

volatile uint8_t  offset_side_local = 0;   // 0: RIGHT, 1: LEFT (프로젝트 정의에 맞게 사용)
volatile uint16_t offset_avg_local  = 0;


extern uint8_t  offset_side;
extern uint16_t offset_average;


void line_tracing_init(const lt_config_t *cfg)
{
    g_cfg = *cfg;
    g_enabled   = false;
    prev_error  = 0.0f;
    integral    = 0.0f;
    prev_ms     = 0;

    offset_avg_local = offset_average;
    offset_side_local = offset_side;
}

void line_tracing_enable(bool on)
{
    g_enabled = on;

    if (on)
    {
        prev_error = 0.0f;
        integral   = 0.0f;
        prev_ms    = 0;

        // 구동 가능 상태
        step_set_hold(HOLD_BRAKE);
        step_drive(OP_FORWARD);
    }
    else
    {
        step_coast_stop();
    }
}

bool line_tracing_enabled(void)
{
    return g_enabled;
}

void line_tracing_set_offset(uint8_t side, uint16_t avg)
{
    offset_side_local = side;
    offset_avg_local  = avg;
}

void line_tracing_set_gains(float kp, float ki, float kd)
{
    g_cfg.Kp = kp;
    g_cfg.Ki = ki;
    g_cfg.Kd = kd;
}

void line_tracing_update(uint32_t now_ms)
{
    if (!g_enabled)
    {
        return;
    }

    if (g_cfg.interval_ms == 0)
    {
        g_cfg.interval_ms = 5;
    }

    if (prev_ms && (uint32_t)(now_ms - prev_ms) < g_cfg.interval_ms)
    {
        return;
    }

    prev_ms = now_ms;

    // ── 센서 읽기(폴링; ISR에서 호출 금지) ─────────────────────────────
    bh1749_color_data_t L = bh1749_read_rgbir(BH1749_ADDR_LEFT);
    bh1749_color_data_t R = bh1749_read_rgbir(BH1749_ADDR_RIGHT);

    uint32_t lb = calculate_brightness(L.red, L.green, L.blue);
    uint32_t rb = calculate_brightness(R.red, R.green, R.blue);

    // 오프셋 보정(측정 환경 따라 한쪽만 기준치 빼기)
    if (offset_side_local/* == LEFT*/)   // LEFT 기준
    {
        if (lb > offset_avg_local)
        {
            lb -= offset_avg_local;
        }
    }
    else                   			 // RIGHT 기준
    {
        if (rb > offset_avg_local)
        {
            rb -= offset_avg_local;
        }
    }

    // ── PID ────────────────────────────────────────────────────────────
    float error      = (float)rb - (float)lb;
    // integral      += error * dt;  // dt가 필요하면 interval_ms 사용
    float derivative = error - prev_error;
    float output     = g_cfg.Kp * error + g_cfg.Ki * integral + g_cfg.Kd * derivative;
    prev_error       = error;

    // ── 속도(=period ticks) 계산 ───────────────────────────────────────
    uint32_t left_ticks  = (float)g_cfg.base_ticks + output;
    uint32_t right_ticks = (float)g_cfg.base_ticks - output;

    // 데드존 완화
    if (abs((int)output) < 200)
    {
        left_ticks  = (float)g_cfg.base_ticks;
        right_ticks = (float)g_cfg.base_ticks;
    }

    // 한계 클램프
    if (left_ticks  < g_cfg.min_ticks) left_ticks  = (float)g_cfg.min_ticks;
    if (right_ticks < g_cfg.min_ticks) right_ticks = (float)g_cfg.min_ticks;
    if (left_ticks  > g_cfg.max_ticks) left_ticks  = (float)g_cfg.max_ticks;
    if (right_ticks > g_cfg.max_ticks) right_ticks = (float)g_cfg.max_ticks;

    // ── 조향(느리면 제자리 턴) ────────────────────────────────────────
    if (left_ticks < 800.0f || right_ticks < 800.0f)
    {
        step_drive((left_ticks < right_ticks) ? OP_TURN_RIGHT : OP_TURN_LEFT);
    }
    else
    {
        step_drive(OP_FORWARD);
    }

    step_drive_ratio((uint16_t)left_ticks, (uint16_t)right_ticks);

	uart_printf("g_cfg_base_ticks: %d\r\n", g_cfg.base_ticks);
	uart_printf("g_cfg_min_ticks: %d\r\n", g_cfg.min_ticks);
	uart_printf("g_cfg_max_ticks: %d\r\n", g_cfg.max_ticks);
	uart_printf("left_ticks: %d | right_ticks: %d\r\n", left_ticks, right_ticks);

    // (옵션) 디버깅
    // uart_printf("[LT] lb:%lu rb:%lu err:%0.2f out:%0.2f L:%0.1f R:%0.1f\r\n",
    //             (unsigned long)lb, (unsigned long)rb, error, output, left_ticks, right_ticks);
}
