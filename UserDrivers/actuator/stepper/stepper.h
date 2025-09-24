/*
 * stepper.h
 *
 *  Created on: Sep 10, 2025
 *      Author: RCY
 */

#ifndef ACTUATOR_STEPPER_STEPPER_H_
#define ACTUATOR_STEPPER_STEPPER_H_

#include "def.h"
//#include "color.h"


/* ================= STM32U375RGT6 Pin Map =================
 * 15BY25-729 + A3916
 *  MCU  |   NET      | DRIVER
 * PA11 -> MOT_L_IN1  -> IN1
 * PA10 -> MOT_L_IN2  -> IN2
 * PA9  -> MOT_L_IN3  -> IN3
 * PA8  -> MOT_L_IN4  -> IN4
 *
 * PC9  -> MOT_R_IN1  -> IN1
 * PC8  -> MOT_R_IN2  -> IN2
 * PC7  -> MOT_R_IN3  -> IN3
 * PC6  -> MOT_R_IN4  -> IN4
 * ======================================================== */

#define L_IN1_PORT GPIOA
#define L_IN1_PIN  GPIO_PIN_11
#define L_IN2_PORT GPIOA
#define L_IN2_PIN  GPIO_PIN_10
#define L_IN3_PORT GPIOA
#define L_IN3_PIN  GPIO_PIN_9
#define L_IN4_PORT GPIOA
#define L_IN4_PIN  GPIO_PIN_8

#define R_IN1_PORT GPIOC
#define R_IN1_PIN  GPIO_PIN_9
#define R_IN2_PORT GPIOC
#define R_IN2_PIN  GPIO_PIN_8
#define R_IN3_PORT GPIOC
#define R_IN3_PIN  GPIO_PIN_7
#define R_IN4_PORT GPIOC
#define R_IN4_PIN  GPIO_PIN_6

#define SAFE_MAX_RPM 1200
#define MAX_SPEED 100
#define MIN_SPEED 0


// Select step mode (FULL / HALF / MICRO)
#define _STEP_MODE_FULL 0
#define _STEP_MODE_HALF 1
#define _STEP_MODE_MICRO 2


#ifndef _USE_STEP_MODE
#define _USE_STEP_MODE _STEP_MODE_MICRO
#endif


#if (_USE_STEP_MODE == _STEP_MODE_HALF)
#define STEP_MASK 0x07
#define STEP_PER_REV 40
#define STEP_TABLE_SIZE 8
#elif (_USE_STEP_MODE == _STEP_MODE_FULL)
#define STEP_MASK 0x03
#define STEP_PER_REV 20
#define STEP_TABLE_SIZE 4
#elif (_USE_STEP_MODE == _STEP_MODE_MICRO)
#define STEP_MASK 0x1F
#define STEP_PER_REV 80
#define STEP_TABLE_SIZE 32
#else
#error "_USE_STEP_MODE invalid"
#endif


// 기어/모터 세트 선택
#define _STEP_NUM_119 0 // 15BY25-119 (gearless; CW sign differs)
#define _STEP_NUM_728 1 // 10:1
#define _STEP_NUM_729 2 // 20:1
#ifndef _USE_STEP_NUM
#define _USE_STEP_NUM _STEP_NUM_729
#endif


// 선택: SW PWM vs HW PWM
#define PWM_IMPL_SOFT 0
#define PWM_IMPL_HARD 1
#ifndef _PWM_IMPL
#define _PWM_IMPL PWM_IMPL_HARD   // 기본은 기존 SW PWM 유지
#endif

// ---------------- Types ----------------
// ISR‑safe POD struct for one motor
typedef struct
{
	GPIO_TypeDef* in1p; 	uint16_t in1b; // bit mask (e.g., GPIO_PIN_0)
	GPIO_TypeDef* in2p; 	uint16_t in2b;
	GPIO_TypeDef* in3p; 	uint16_t in3b;
	GPIO_TypeDef* in4p; 	uint16_t in4b;


	volatile uint16_t 	step_idx; // 0..STEP_MASK (wraps via mask)
	volatile uint32_t 	period_ticks; // tick source = TIMx (1us or similar)
	volatile uint32_t 	prev_tick; // last index advance time
	volatile uint32_t 	odometry_steps; // for telemetry (atomic on M4)
			 int8_t 	dir_sign; // +1 / -1 (compile to single add)
} StepLL;

typedef enum
{
	HOLD_OFF 	= 0,	//coils off(freewheel)
	HOLD_BRAKE	= 1		//coils energized(hold torque)
}hold_mode_t;


// High‑level operations kept for API compatibility
typedef enum {
	OP_NONE = 0,
	OP_FORWARD,
	OP_REVERSE,
	OP_TURN_LEFT,
	OP_TURN_RIGHT,
	OP_STOP
} StepOperation;


// ---------------- Public API ----------------
// 1) Init & wiring
void step_init_all(void);
void step_idx_init(void);


// 2) Real‑time tick: call from your 10us ISR (or whichever period you run the motor ISR)
void step_tick_isr(void);


// 3) Control from main thread (non‑ISR)
void step_set_period_ticks(uint32_t left_ticks, uint32_t right_ticks); // unit = tick source period
void step_set_dir(int8_t left_sign, int8_t right_sign); // +1 or -1
void step_stop(void); // brake both
void step_coast_stop(void);
void step_set_hold(hold_mode_t mode);


// 4) Telemetry
uint32_t get_executed_steps(void);
void odometry_steps_init(void);


// 5) Compatibility helpers (keep your existing higher‑level code working)
void step_drive(StepOperation op);
void step_drive_ratio(uint16_t left_ticks, uint16_t right_ticks);


// Mappings you already used
//StepOperation 	mode_to_step(color_mode_t mode);
//uint16_t 		mode_to_step_count(color_mode_t mode);
//uint16_t 		mode_to_left_period(color_mode_t mode);
//uint16_t 		mode_to_right_period(color_mode_t mode);


// Optional util if you still convert from RPM/PWM in main thread
uint32_t rpm_to_period_ticks(uint16_t rpm, uint32_t tick_hz);
uint32_t pwm_to_rpm(uint8_t pwm);


#endif /* ACTUATOR_STEPPER_STEPPER_H_ */
