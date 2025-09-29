/*
 * card_action.c
 *
 *  Created on: Sep 27, 2025
 *      Author: fbcks
 */


#include "card_action.h"
#include "stepper.h"
#include "uart.h"
#include "btn.h"


// === 동작 스텝수 (튜닝 가능) ===
#define CARD_STEPS_FORWARD     2000u
#define CARD_STEPS_BACKWARD    2000u
#define CARD_STEPS_LEFT        1100u
#define CARD_STEPS_RIGHT       1100u

typedef struct
{
    uint32_t start_steps;
    uint32_t goal_steps;
    uint8_t  running;
} card_plan_t;

static card_plan_t   s_plan;
static StepOperation s_cur_op = OP_STOP;
static mode_sw_t     s_mode   = MODE_INVALID;   // MODE_CARD일 때만 동작

static inline uint32_t steps_now(void)
{
    return get_executed_steps();
}

static inline uint32_t diff_u32(uint32_t a, uint32_t b)
{
    return (uint32_t)(a - b);
}

static inline StepOperation color_to_op(color_t c)
{
    // 기본 매핑: GREEN→FWD, RED→REV, YELLOW→RIGHT, BLUE→LEFT
    switch (c)
    {
        case COLOR_GREEN:  return OP_FORWARD;
        case COLOR_RED:    return OP_REVERSE;
        case COLOR_YELLOW: return OP_TURN_RIGHT;
        case COLOR_BLUE:   return OP_TURN_LEFT;
        default:           return OP_STOP;
    }
}

static inline uint32_t color_to_steps(color_t c)
{
    switch (c)
    {
        case COLOR_GREEN:  return CARD_STEPS_FORWARD;
        case COLOR_RED:    return CARD_STEPS_BACKWARD;
        case COLOR_YELLOW: return CARD_STEPS_RIGHT;
        case COLOR_BLUE:   return CARD_STEPS_LEFT;
        default:           return 0;
    }
}

static inline void drive_if_needed(StepOperation op)
{
    if (op != s_cur_op)
    {
        step_drive(op);
        s_cur_op = op;
    }
}

static void plan_start(StepOperation op, uint32_t target_steps)
{
    if (target_steps == 0 || op == OP_STOP)
        return;

    // 새 구간 시작
    step_stop();
    step_set_hold(HOLD_BRAKE);
    odometry_steps_init();

    s_plan.start_steps = steps_now();
    s_plan.goal_steps  = target_steps;
    s_plan.running     = 1;

    drive_if_needed(op);

    uart_printf("[CARD-ACT] start op=%d goal=%lu\r\n",
                (int)op, (unsigned long)target_steps);
}

void card_action_init(void)
{
    s_plan.start_steps = 0;
    s_plan.goal_steps  = 0;
    s_plan.running     = 0;
    s_cur_op           = OP_STOP;
    s_mode             = MODE_INVALID;

    step_drive(OP_STOP);
    step_set_hold(HOLD_BRAKE);

    uart_printf("[CARD-ACT] init\r\n");
}

void card_action_set_mode(mode_sw_t m)
{
    s_mode = m;

    if (s_mode != MODE_CARD)
    {
        // 카드 모드가 아니면 즉시 정지
        if (s_plan.running)
        {
            s_plan.running = 0;
            step_stop();
            step_set_hold(HOLD_BRAKE);
            s_cur_op = OP_STOP;
            uart_printf("[CARD-ACT] stop (mode change)\r\n");
        }
    }
}

void card_action_stop(void)
{
    s_plan.running = 0;
    s_cur_op = OP_STOP;
    step_stop();
    step_set_hold(HOLD_BRAKE);
    uart_printf("[CARD-ACT] stop\r\n");
}

void card_action_on_dual_equal(uint8_t left, uint8_t right)
{
    if (s_mode != MODE_CARD)
        return;

    if (left >= COLOR_COUNT || right >= COLOR_COUNT)
        return;

    if (left != right)
        return; // 좌/우 동일하지 않으면 무시

    color_t c = (color_t)left;

    // 단발: 유효 색(G/R/Y/B)만 실행
    StepOperation op = color_to_op(c);
    uint32_t      st = color_to_steps(c);

    if (op != OP_STOP && st > 0)
    {
        card_action_plan(c);  // 동일 로직 재사용
    }
}

void card_action_plan(color_t c)
{
    if (s_mode != MODE_CARD)
        return;

    StepOperation op = color_to_op(c);
    uint32_t      st = color_to_steps(c);

    if (op == OP_STOP || st == 0)
    {
        // 지원하지 않는 색은 무시
        return;
    }

    plan_start(op, st);
}

void card_action_service(void)
{
    if (s_mode != MODE_CARD)
    {
        // 안전: 모드 이탈 시엔 항상 STOP
        if (s_plan.running)
            card_action_stop();
        return;
    }

    if (!s_plan.running)
        return;

    if (diff_u32(steps_now(), s_plan.start_steps) >= s_plan.goal_steps)
    {
        card_action_stop();   // 완료 시 자동 정지
        uart_printf("[CARD-ACT] done\r\n");
    }
}

bool card_action_is_running(void)
{
    return s_plan.running != 0;
}

uint32_t card_action_get_goal_steps(void)
{
    return s_plan.goal_steps;
}
