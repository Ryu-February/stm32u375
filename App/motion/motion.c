/*
 * motion.c
 *
 *  Created on: Sep 17, 2025
 *      Author: RCY
 */

#include "motion.h"
#include "rgb.h"
#include "stepper.h"
#include "uart.h"


extern volatile bool stepper_enable_evt;
extern volatile uint8_t detected_color;
extern volatile bool plan_armed;    // 1초 타이머 통과했는지 표시
extern volatile bool after_1s_evt;


static inline StepOperation color_to_op(color_t c)
{
	switch (c)
	{
		case COLOR_RED:      return OP_REVERSE;     // 좌회전
		case COLOR_YELLOW:   return OP_TURN_RIGHT;  // 전진
		case COLOR_GREEN:    return OP_FORWARD;     // 우회전
		case COLOR_BLUE:     return OP_TURN_LEFT;   // 후진
		case COLOR_PURPLE:	 return OP_FORWARD;		//전진(라인트레이싱)
		default:             return OP_STOP;        // ORANGE/PURPLE/... 전부 STOP
	}
}

static const uint16_t kStepsByColor[COLOR_COUNT] =
{
    [COLOR_RED]      = 2000,
    [COLOR_YELLOW]   = 1050,
    [COLOR_GREEN]    = 2000,
    [COLOR_BLUE]     = 1050,
	[COLOR_PURPLE]	 = 15000,
    // ORANGE, PURPLE, ... 등은 0으로 남음
};

typedef struct
{
	uint32_t	start_steps;
	uint32_t 	goal_steps;
	uint8_t 	running;
}motion_plan_t;

static motion_plan_t g_plan;

static inline uint32_t steps_now(void)
{
	return get_executed_steps();
}


static inline uint32_t diff_u32(uint32_t a, uint32_t b)
{
	return (uint32_t)(a - b);
}

void motion_plan_color(color_t c)
{
	if((unsigned)c >= COLOR_COUNT)
		return;

	StepOperation op = color_to_op(c);
	uint16_t goal	 = kStepsByColor[c];

	if (op == OP_STOP || goal == 0)
	{
		step_stop();
		step_set_hold(HOLD_BRAKE);
		g_plan.running = 0;
		stepper_enable_evt = false;
		after_1s_evt = false;
		plan_armed = false;
		detected_color = COLOR_BLACK;
		return;
	}
//	step_idx_init();
//	odometry_steps_init();
    // 방향/주행 시작
	step_stop();
    step_set_hold(HOLD_BRAKE);
    step_drive(op);

    g_plan.start_steps = steps_now();
    g_plan.goal_steps  = goal;
    g_plan.running     = 1;
}


void motion_service(void)
{
    if (!g_plan.running)
        return;

    if (diff_u32(steps_now(), g_plan.start_steps) >= g_plan.goal_steps)
    {
    	step_stop();
        step_set_hold(HOLD_BRAKE); // 또는 HOLD_OFF
        g_plan.running = 0;
		stepper_enable_evt = false;
		detected_color = COLOR_BLACK;
		plan_armed = false;
		after_1s_evt = false;
    }
}


bool motion_is_running(void)
{
    // volatile 읽기: 단순 읽기만 해도 충분
    return g_plan.running;
}

uint32_t get_goal_steps(void)
{
	return g_plan.goal_steps;
}
