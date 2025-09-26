/*
 * btn_action.c
 *
 *  Created on: Sep 26, 2025
 *      Author: RCY
 */


#include "btn_action.h"
#include "stepper.h"
#include "uart.h"
#include "btn.h"


// 각 버튼에 대응하는 목표 스텝(필요시 조정)
#define BTN_STEPS_FORWARD     2000u
#define BTN_STEPS_BACKWARD    2000u
#define BTN_STEPS_LEFT        1050u
#define BTN_STEPS_RIGHT       1050u

typedef struct
{
    uint32_t start_steps;
    uint32_t goal_steps;
    uint8_t  running;
} btn_plan_t;

static btn_plan_t   s_plan;
static StepOperation s_cur_op = OP_STOP;

static inline uint32_t steps_now(void)
{
    return get_executed_steps();
}

static inline uint32_t diff_u32(uint32_t a, uint32_t b)
{
    return (uint32_t)(a - b);
}

static inline StepOperation btn_to_op(btn_id_t b)
{
    switch (b)
    {
        case BTN_FORWARD:   return OP_FORWARD;
        case BTN_BACKWARD:  return OP_REVERSE;
        case BTN_LEFT:      return OP_TURN_LEFT;
        case BTN_RIGHT:     return OP_TURN_RIGHT;
        default:            return OP_STOP;    // GO/DELETE/RESUME 등은 STOP 처리
    }
}

static inline uint32_t btn_to_steps(btn_id_t b)
{
    switch (b)
    {
        case BTN_FORWARD:   return BTN_STEPS_FORWARD;
        case BTN_BACKWARD:  return BTN_STEPS_BACKWARD;
        case BTN_LEFT:      return BTN_STEPS_LEFT;
        case BTN_RIGHT:     return BTN_STEPS_RIGHT;
        default:            return 0;
    }
}

void btn_action_init(void)
{
    s_plan.start_steps = 0;
    s_plan.goal_steps  = 0;
    s_plan.running     = 0;
    s_cur_op           = OP_STOP;
    step_drive(OP_STOP);
    step_set_hold(HOLD_BRAKE);
}

void btn_action_plan(btn_id_t b)
{
    StepOperation op = btn_to_op(b);
    uint32_t goal    = btn_to_steps(b);

    if (op == OP_STOP || goal == 0)
    {
        btn_action_stop();
        return;
    }

    // 새 동작 시작
    step_stop();
    step_set_hold(HOLD_BRAKE);
    odometry_steps_init();

    s_plan.start_steps = steps_now();
    s_plan.goal_steps  = goal;
    s_plan.running     = 1;

    step_drive(op);
    s_cur_op = op;

    uart_printf("[BTN-ACT] start op=%d goal=%lu\r\n",
                (int)op, (unsigned long)goal);
}

void btn_action_service(void)
{
    if (!s_plan.running)
        return;

    if (diff_u32(steps_now(), s_plan.start_steps) >= s_plan.goal_steps)
    {
        step_stop();
        step_set_hold(HOLD_BRAKE);      // 필요하면 HOLD_OFF
        s_plan.running = 0;
        s_cur_op = OP_STOP;
        uart_printf("[BTN-ACT] done\r\n");
    }
}

void btn_action_stop(void)
{
    s_plan.running = 0;
    s_cur_op = OP_STOP;
    step_stop();
    step_set_hold(HOLD_BRAKE);
    uart_printf("[BTN-ACT] stop\r\n");
}

bool btn_action_is_running(void)
{
    return s_plan.running != 0;
}

uint32_t btn_action_get_goal_steps(void)
{
    return s_plan.goal_steps;
}
