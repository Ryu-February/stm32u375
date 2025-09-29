/*
 * stepper.c
 *
 *  Created on: Sep 10, 2025
 *      Author: RCY
 */


#include "stepper.h"

#include "uart.h"

/*                            Motor(15BY25-729)                         */
/*                           Motor driver(A3916)                        */

/************************************************************************/
/*                       MCU  |     NET    | DRIVER                     */
/*                      PA11 -> MOT_L_IN1 ->  IN1                       */
/*                      PA10 -> MOT_L_IN2 ->  IN2                       */
/*                      PA9  -> MOT_L_IN3 ->  IN3                       */
/*                      PA8  -> MOT_L_IN4 ->  IN4                       */
/*                                                                      */
/*                      PC9  -> MOT_R_IN1 ->  IN1                       */
/*                      PC8  -> MOT_R_IN2 ->  IN2                       */
/*                      PC7  -> MOT_R_IN3 ->  IN3                       */
/*                      PC6  -> MOT_R_IN4 ->  IN4                       */
/************************************************************************/

/*
 * A3916 Stepper Motor Operation Table (Half Step + Full Step)
 *
 * IN1 IN2 IN3 IN4 | OUT1A OUT1B OUT2A OUT2B | Function
 * --------------------------------------------------------
 *  0   0   0   0  |  Off   Off   Off   Off   | Disabled
 *  1   0   1   0  |  High  Low   High  Low   | Full Step 1 / 	½ Step 1
 *  0   0   1   0  |  Off   Off   High  Low   |             	½ Step 2
 *  0   1   1   0  |  Low   High  High  Low   | Full Step 2 /	½ Step 3
 *  0   1   0   0  |  Low   High  Off   Off   |             	½ Step 4
 *  0   1   0   1  |  Low   High  Low   High  | Full Step 3 / 	½ Step 5
 *  0   0   0   1  |  Off   Off   Low   High  |             	½ Step 6
 *  1   0   0   1  |  High  Low   Low   High  | Full Step 4 / 	½ Step 7
 *  1   0   0   0  |  High  Low   Off   Off   |             	½ Step 8
 */


// ---- External timers (provide from your BSP) ----
// TIM_PWM_CNT: ARR must be 255; we only read CNT (0..255) for software PWM compare
// TIM_TICK : free‑running tick for step period (e.g., 1us per tick)
//extern TIM_HandleTypeDef htim16; // use as tick source (1us CNT)
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;



#define TIM_PWM_CNT ((uint32_t)(TIM2->CNT)) // use as PWM CNT (ARR=255)
#define TIM_TICK_CNT (read_tick32())

static inline uint32_t read_tick32(void){ return TIM2->CNT; }


// ---- Global motors ----
static StepLL left = {
	.in1p = L_IN1_PORT, .in1b = L_IN1_PIN,
	.in2p = L_IN2_PORT, .in2b = L_IN2_PIN,
	.in3p = L_IN3_PORT, .in3b = L_IN3_PIN,
	.in4p = L_IN4_PORT, .in4b = L_IN4_PIN,

	.step_idx = 0,
	.period_ticks = 500,
	.prev_tick = 0,
	.odometry_steps = 0,
#if (_USE_STEP_NUM == _STEP_NUM_119)
	.dir_sign = -1,
#else
	.dir_sign = +1,
#endif
};


static StepLL right = {
	.in1p = R_IN1_PORT, .in1b = R_IN1_PIN,
	.in2p = R_IN2_PORT, .in2b = R_IN2_PIN,
	.in3p = R_IN3_PORT, .in3b = R_IN3_PIN,
	.in4p = R_IN4_PORT, .in4b = R_IN4_PIN,

	.step_idx = 0,
	.period_ticks = 500,
	.prev_tick = 0,
	.odometry_steps = 0,
	#if (_USE_STEP_NUM == _STEP_NUM_119)
	.dir_sign = +1,
	#else
	.dir_sign = -1,
	#endif
};


// Run/hold flags
static volatile uint8_t left_run = 0;
static volatile uint8_t right_run = 0;
static volatile hold_mode_t g_hold = HOLD_BRAKE;

// ---- LUTs ----

//sin table
//360도를 32스텝으로 쪼갰을 때 11.25가 나오는데 11.25도의 간격을 pwm으로 표현하면 이렇게 나옴
//여기서 말하는 360은 전류 벡터의 회전 경로가 360도라는 거고 모터는 동일하게 72도를 기준으로 잡음
#if (_USE_STEP_MODE == _STEP_MODE_MICRO)
	const uint8_t step_table[32] = {			//sin(degree) -> pwm
	  128, 152, 176, 198, 218, 234, 245, 253,
	  255, 253, 245, 234, 218, 198, 176, 152,
	  128, 103,  79,  57,  37,  21,  10,   2,
		0,   2,  10,  21,  37,  57,  79, 103
	};
#elif (_USE_STEP_MODE == _STEP_MODE_FULL)
	static const uint8_t step_table[4][4] = {
		//72°를 돎(step angle이 18°라서)
		{1,0,1,0},
		{0,1,1,0},
		{0,1,0,1},
		{1,0,0,1}
	};
#else // HALF
	static const uint8_t step_table[8][4] = {
		{1,0,1,0},
		{0,0,1,0},
		{0,1,1,0},
		{0,1,0,0},
		{0,1,0,1},
		{0,0,0,1},
		{1,0,0,1},
		{1,0,0,0}
	};
#endif


// ---- Helpers ----
static inline uint32_t diff_u32(uint32_t a, uint32_t b)
{
	return (uint32_t)(a - b);
}


static inline void gpio_pwm4(
	GPIO_TypeDef* p1, uint16_t b1,
	GPIO_TypeDef* p2, uint16_t b2,
	GPIO_TypeDef* p3, uint16_t b3,
	GPIO_TypeDef* p4, uint16_t b4,
	uint8_t now, uint8_t vA, uint8_t vB)
{
	uint32_t set1 = (now < vA) ? b1 : 0, 					rst1 = (now < vA) ? 0 : ((uint32_t)b1 << 16);
	uint32_t set2 = (now < (uint8_t)(255 - vA)) ? b2 : 0, 	rst2 = (now < (uint8_t)(255 - vA)) ? 0 : ((uint32_t)b2 << 16);
	uint32_t set3 = (now < vB) ? b3 : 0, 					rst3 = (now < vB) ? 0 : ((uint32_t)b3 << 16);
	uint32_t set4 = (now < (uint8_t)(255 - vB)) ? b4 : 0, 	rst4 = (now < (uint8_t)(255 - vB)) ? 0 : ((uint32_t)b4 << 16);

	p1->BSRR = set1 | rst1;//A+
	p2->BSRR = set2 | rst2;//A-
	p3->BSRR = set3 | rst3;//B+
	p4->BSRR = set4 | rst4;//B-
}


#if (_PWM_IMPL == PWM_IMPL_HARD)

static volatile uint8_t s_pwm_forced = 0; // 1: OCMODE_FORCED_* 상태

static void hwpwm_set_ocmode_all(TIM_HandleTypeDef* htim, uint32_t mode)
{
    TIM_OC_InitTypeDef s = {0};
    s.OCMode       = mode;                   // TIM_OCMODE_PWM1 / TIM_OCMODE_FORCED_ACTIVE / TIM_OCMODE_FORCED_INACTIVE
    s.Pulse        = 0;                      // 모드에 따라 무시됨
    s.OCPolarity   = TIM_OCPOLARITY_HIGH;    // 모두 동일 극성 보장
    s.OCFastMode   = TIM_OCFAST_DISABLE;

    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_1);
    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_2);
    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_3);
    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_4);

    // 재적용
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_1);
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_2);
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_3);
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_4);
}

static void hwpwm_use_pwm_mode(void)
{
    hwpwm_set_ocmode_all(&htim1, TIM_OCMODE_PWM1);
    hwpwm_set_ocmode_all(&htim3, TIM_OCMODE_PWM1);
    s_pwm_forced = 0; // ← 복귀 완료
}

static void hwpwm_brake(void)
{
    hwpwm_set_ocmode_all(&htim1, TIM_OCMODE_FORCED_ACTIVE);
    hwpwm_set_ocmode_all(&htim3, TIM_OCMODE_FORCED_ACTIVE);
    s_pwm_forced = 1; // ← FORCED 진입
}

static void hwpwm_coast(void)
{
    hwpwm_set_ocmode_all(&htim1, TIM_OCMODE_FORCED_INACTIVE);
    hwpwm_set_ocmode_all(&htim3, TIM_OCMODE_FORCED_INACTIVE);
    s_pwm_forced = 1; // ← FORCED 진입
}


// CCR에 0..255 값 바로 쓰기 (ARR=255 가정)
static inline void hwpwm_set_left(uint8_t a_plus, uint8_t a_minus,
                                  uint8_t b_plus, uint8_t b_minus)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, a_plus);   // PA8  A+
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, a_minus);  // PA9  A-
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, b_plus);   // PA10 B+
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, b_minus);  // PA11 B-
}

static inline void hwpwm_set_right(uint8_t a_plus, uint8_t a_minus,
                                   uint8_t b_plus, uint8_t b_minus)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, a_plus);   // PC6  A+
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, a_minus);  // PC7  A-
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, b_plus);   // PC8  B+
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, b_minus);  // PC9  B-
}
#endif

static inline void apply_pwm_micro(StepLL* m, uint8_t now)
{
#if (_USE_STEP_MODE == _STEP_MODE_MICRO)
	uint8_t vA = step_table[m->step_idx & STEP_MASK];
	uint8_t vB = step_table[(m->step_idx + (STEP_TABLE_SIZE >> 2)) & STEP_MASK];
	//(STEP_MASK >> 2) == 8 == 90°(difference sin with cos)
	//sin파와 cos파의 위상 차가 90도가 나니까 +8을 한 거임 +8은 32를 360도로 치환했을 때 90도를 의미함


#if (_PWM_IMPL == PWM_IMPL_SOFT)
    // 기존 소프트웨어 PWM (BSRR 토글)
    gpio_pwm4(m->in1p, m->in1b, m->in2p, m->in2b, m->in3p, m->in3b, m->in4p, m->in4b,
              now, vA, vB);
#else
    // 하드웨어 PWM: 채널 4개에 vA, 255-vA / vB, 255-vB 적재
    if (m == &left)
        hwpwm_set_left(vA, (uint8_t)(255 - vA), vB, (uint8_t)(255 - vB));
    else
        hwpwm_set_right(vA, (uint8_t)(255 - vA), vB, (uint8_t)(255 - vB));
#endif
#else
	(void)m; (void)now; // silent
#endif
}




static inline void apply_coils_table(StepLL* m)
{
#if (_USE_STEP_MODE != _STEP_MODE_MICRO)
    const uint8_t* s = step_table[m->step_idx & STEP_MASK]; // {A+,A-,B+,B-} = {0/1}

#if (_PWM_IMPL == PWM_IMPL_SOFT)
    uint32_t set1 = s[0]? m->in1b:0, rst1 = s[0]?0:((uint32_t)m->in1b<<16);
    uint32_t set2 = s[1]? m->in2b:0, rst2 = s[1]?0:((uint32_t)m->in2b<<16);
    uint32_t set3 = s[2]? m->in3b:0, rst3 = s[2]?0:((uint32_t)m->in3b<<16);
    uint32_t set4 = s[3]? m->in4b:0, rst4 = s[3]?0:((uint32_t)m->in4b<<16);
    m->in1p->BSRR = set1 | rst1;
    m->in2p->BSRR = set2 | rst2;
    m->in3p->BSRR = set3 | rst3;
    m->in4p->BSRR = set4 | rst4;
#else
    uint8_t Aplus  = s[0] ? 255 : 0;
    uint8_t Aminus = s[1] ? 255 : 0;
    uint8_t Bplus  = s[2] ? 255 : 0;
    uint8_t Bminus = s[3] ? 255 : 0;

    if (m == &left)  hwpwm_set_left(Aplus, Aminus, Bplus, Bminus);
    else             hwpwm_set_right(Aplus, Aminus, Bplus, Bminus);
#endif

#endif
}


static inline void try_advance(StepLL* m, uint32_t now_tick)
{
	if (diff_u32(now_tick, m->prev_tick) >= m->period_ticks)
	{
		m->prev_tick = now_tick;
		m->odometry_steps++;
		m->step_idx = (uint16_t)((m->step_idx + m->dir_sign) & STEP_MASK);
	}
}

// ---- Public Impl ----
void step_init_all(void)
{
	left.step_idx = right.step_idx = 0;
	left.prev_tick = right.prev_tick = read_tick32();
	left.odometry_steps = right.odometry_steps = 0;
	left_run = right_run = 0;
	g_hold = HOLD_BRAKE;

#if (_PWM_IMPL == PWM_IMPL_HARD)
    // 타이머 PWM 스타트 (PSC=17, ARR=255 는 CubeMX에서 설정되어 있다고 가정)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
#endif

	step_stop();
	step_drive(OP_STOP);
}


void step_idx_init(void)
{
	left.step_idx = right.step_idx = 0;
}


void step_tick_isr(void)
{
#if (_PWM_IMPL == PWM_IMPL_SOFT)
    uint8_t  pwm_now  = (uint8_t)TIM_PWM_CNT; // 0..255
#else
    uint8_t  pwm_now  = 0; // HW PWM에서는 사용 안 함
#endif
    uint32_t tick_now = read_tick32();

    if (g_hold == HOLD_OFF)
    {
#if (_PWM_IMPL == PWM_IMPL_SOFT)
        // 기존: 출력 Low
        left.in1p->BSRR = ((uint32_t)left.in1b << 16);
        left.in2p->BSRR = ((uint32_t)left.in2b << 16);
        left.in3p->BSRR = ((uint32_t)left.in3b << 16);
        left.in4p->BSRR = ((uint32_t)left.in4b << 16);

        right.in1p->BSRR = ((uint32_t)right.in1b << 16);
        right.in2p->BSRR = ((uint32_t)right.in2b << 16);
        right.in3p->BSRR = ((uint32_t)right.in3b << 16);
        right.in4p->BSRR = ((uint32_t)right.in4b << 16);
#else
        // HW: 모든 CCR=0
        hwpwm_set_left(0,0,0,0);
        hwpwm_set_right(0,0,0,0);
#endif
        return;
    }

#if (_USE_STEP_MODE == _STEP_MODE_MICRO)
    apply_pwm_micro(&left,  pwm_now);
    apply_pwm_micro(&right, pwm_now);
#else
    apply_coils_table(&left);
    apply_coils_table(&right);
#endif

    try_advance(&left,  tick_now);
    try_advance(&right, tick_now);
}


void step_set_period_ticks(uint32_t left_ticks, uint32_t right_ticks)
{
	left.period_ticks = left_ticks ? left_ticks : 1;
	right.period_ticks = right_ticks ? right_ticks : 1;
}


static inline int8_t sgn3(int8_t s)
{
	return (s > 0) ? +1 : (s < 0 ? -1 : 0);
}

void step_set_dir(int8_t left_sign, int8_t right_sign)
{
#if (_USE_STEP_NUM == _STEP_NUM_119)
    const int8_t L = -1;   // left motor polarity
    const int8_t R = +1;   // right motor polarity
#else
    const int8_t L = +1;
    const int8_t R = -1;
#endif

    left.dir_sign  = (int8_t)(sgn3(left_sign) * L);
    right.dir_sign = (int8_t)(sgn3(right_sign) * R);
}


void step_stop(void)
{
//	// brake = 4핀 모두 High (A3916 보드 동작 확인 필요)
//	left.in1p->BSRR = left.in1b;
//	left.in2p->BSRR = left.in2b;
//	left.in3p->BSRR = left.in3b;
//	left.in4p->BSRR = left.in4b;
//
//
//	right.in1p->BSRR = right.in1b;
//	right.in2p->BSRR = right.in2b;
//	right.in3p->BSRR = right.in3b;
//	right.in4p->BSRR = right.in4b;

	// Stop advancing only. Hold mode is respected by ISR.
	left_run = right_run = 0;
}



void step_set_hold(hold_mode_t mode)
{
	g_hold = mode;

#if (_PWM_IMPL == PWM_IMPL_SOFT)
	if (g_hold == HOLD_OFF)// Coast: INx=0,0
	{
		// Immediately de-energize coils
		left.in1p->BSRR = ((uint32_t)left.in1b << 16);
		left.in2p->BSRR = ((uint32_t)left.in2b << 16);
		left.in3p->BSRR = ((uint32_t)left.in3b << 16);
		left.in4p->BSRR = ((uint32_t)left.in4b << 16);

		right.in1p->BSRR = ((uint32_t)right.in1b << 16);
		right.in2p->BSRR = ((uint32_t)right.in2b << 16);
		right.in3p->BSRR = ((uint32_t)right.in3b << 16);
		right.in4p->BSRR = ((uint32_t)right.in4b << 16);
	}
	else   // Short-Brake: INx=1,1  (A3916 브레이크 모드)
	{
		left.in1p->BSRR = left.in1b;
		left.in2p->BSRR = left.in2b;
		left.in3p->BSRR = left.in3b;
		left.in4p->BSRR = left.in4b;

		right.in1p->BSRR = right.in1b;
		right.in2p->BSRR = right.in2b;
		right.in3p->BSRR = right.in3b;
		right.in4p->BSRR = right.in4b;
	}
#else
    if (g_hold == HOLD_OFF)
    {
        // 코스트: CCR=0
//        hwpwm_set_left(0,0,0,0);
//        hwpwm_set_right(0,0,0,0);
        hwpwm_coast();
    }
    else
    {
        // 브레이크: A+/A- & B+/B- 모두 255로 고정(정지 토크)
//        hwpwm_set_left(255,255,255,255);
//        hwpwm_set_right(255,255,255,255);
        hwpwm_brake();
    }
#endif
}

void step_coast_stop(void)
{
	step_stop();				//run = 0
	step_set_hold(HOLD_BRAKE);	//coils off
}


uint32_t get_executed_steps(void)
{
	uint32_t l = left.odometry_steps; // single 32b read is atomic on M4
	uint32_t r = right.odometry_steps;
	return (l + r) / 2u;
}


void odometry_steps_init(void)
{
	left.odometry_steps = right.odometry_steps = 0;
}


// ---- Compatibility helpers ----
void step_drive(StepOperation op)
{
#if (_PWM_IMPL == PWM_IMPL_HARD)
    if (op != OP_STOP && g_hold != HOLD_OFF && s_pwm_forced)
        hwpwm_use_pwm_mode(); // 복귀 보강
#endif
	switch(op)
	{
		case OP_NONE:
			step_set_dir(0, 0);
			break;
		case OP_FORWARD:
			step_set_dir(+1, +1);
			break;
		case OP_REVERSE:
			step_set_dir(-1, -1);
			break;
		case OP_TURN_LEFT:
			step_set_dir(-1, +1);
			break;
		case OP_TURN_RIGHT:
			step_set_dir(+1, -1);
			break;
		case OP_STOP:
			step_set_hold(HOLD_BRAKE);
			break;
		default:
			break;
	}
}


void step_drive_ratio(uint16_t left_ticks, uint16_t right_ticks)
{
	step_set_period_ticks(left_ticks, right_ticks);
}
/*
// ---- Mappings (fixed macro bug: compare _USE_STEP_NUM, not _USE_STEP_MODE) ----
StepOperation mode_to_step(color_mode_t mode)
{
	switch(mode)
	{
		case MODE_FORWARD:
		case MODE_FAST_FORWARD:
		case MODE_SLOW_FORWARD:
		case MODE_LONG_FORWARD:
			return OP_FORWARD;

		case MODE_BACKWARD:
		case MODE_FAST_BACKWARD:
		case MODE_SLOW_BACKWARD:
			return OP_REVERSE;

		case MODE_LEFT:
			return OP_TURN_LEFT;
		case MODE_RIGHT:
			return OP_TURN_RIGHT;
		default:
			return OP_NONE;
	}
}


uint16_t mode_to_step_count(color_mode_t mode)
{
#if (_USE_STEP_NUM == _STEP_NUM_119)
	switch(mode)
	{
		case MODE_FORWARD:
		case MODE_BACKWARD:
		case MODE_FAST_FORWARD:
		case MODE_SLOW_FORWARD:
			return 1000;
		case MODE_FAST_BACKWARD:
		case MODE_SLOW_BACKWARD:
			return 850;
		case MODE_LEFT:
		case MODE_RIGHT:
			return 390;
		case MODE_LONG_FORWARD:
			return 1900;
		case MODE_LINE_TRACE:
			return 30000;
		default:
			return 0;
	}
#elif (_USE_STEP_NUM == _STEP_NUM_729)
	switch(mode)
	{
		case MODE_FORWARD:
		case MODE_BACKWARD:
		case MODE_FAST_FORWARD:
		case MODE_SLOW_FORWARD:
			return 2800;
		case MODE_FAST_BACKWARD:
		case MODE_SLOW_BACKWARD:
			return 2050;
		case MODE_LEFT:
		case MODE_RIGHT:
			return 990;
		case MODE_LONG_FORWARD:
			return 1900;
		case MODE_LINE_TRACE:
			return 30000;
		default:
			return 0;
}
#else // _STEP_NUM_728 or others -> tune as needed
	switch(mode)
	{
		default:
			return 0;
	}
#endif
}

uint16_t mode_to_left_period(color_mode_t mode){
#if (_USE_STEP_NUM == _STEP_NUM_119)
	switch(mode)
	{
		case MODE_FAST_FORWARD: return 2000;
		case MODE_SLOW_FORWARD: return 700;
		case MODE_FAST_BACKWARD: return 1500;
		case MODE_SLOW_BACKWARD: return 1000;
		default: return 1000;
	}
#elif (_USE_STEP_NUM == _STEP_NUM_729)
	switch(mode)
	{
		case MODE_FAST_FORWARD: return 2000;
		case MODE_SLOW_FORWARD: return 700;
		case MODE_FAST_BACKWARD: return 1500;
		case MODE_SLOW_BACKWARD: return 1000;
		default: return 500;
	}
#else
	return 1000;
#endif
}


uint16_t mode_to_right_period(color_mode_t mode)
{
#if (_USE_STEP_NUM == _STEP_NUM_119)
	switch(mode)
	{
		case MODE_FAST_FORWARD: return 700;
		case MODE_SLOW_FORWARD: return 2000;
		case MODE_FAST_BACKWARD: return 1000;
		case MODE_SLOW_BACKWARD: return 1500;
		default: return 500;
	}
#elif (_USE_STEP_NUM == _STEP_NUM_729)
	switch(mode)
	{
		case MODE_FAST_FORWARD: return 700;
		case MODE_SLOW_FORWARD: return 2000;
		case MODE_FAST_BACKWARD: return 1000;
		case MODE_SLOW_BACKWARD: return 1500;
		default: return 500;
	}
#else
	return 500;
#endif
}
*/

// ---- Utils ----
uint32_t pwm_to_rpm(uint8_t pwm)
{
	if (pwm > MAX_SPEED)
		pwm = MAX_SPEED;

	return (uint32_t)pwm * SAFE_MAX_RPM / MAX_SPEED;
}


uint32_t rpm_to_period_ticks(uint16_t rpm, uint32_t tick_hz){
if (rpm == 0) rpm = 1;
if (rpm > SAFE_MAX_RPM) rpm = SAFE_MAX_RPM;
// Period (ticks per index step) = tick_hz / (steps_per_sec)
// steps_per_sec = rpm * STEP_PER_REV / 60
// => ticks = tick_hz * 60 / (rpm * STEP_PER_REV)
uint32_t num = tick_hz * 60UL;
uint32_t den = (uint32_t)rpm * (uint32_t)STEP_PER_REV;
return (den ? (num / den) : num);
}
