/*
 * card_prog.c
 *
 *  Created on: Sep 27, 2025
 *      Author: fbcks
 */

#include "card_prog.h"
#include "uart.h"
#include "rgb_actions.h"


// ====== 내부 타입/상태 ======
typedef struct
{
    card_prog_op_t op;
    uint32_t       target_steps;
} card_item_t;

static card_item_t      	s_buf[CARD_PROG_MAX_LEN];
static uint8_t          	s_len = 0;
static uint8_t          	s_idx = 0;

static card_prog_state_t 	s_state = CARD_PROG_IDLE;
static mode_sw_t        	s_mode  = MODE_INVALID;
static StepOperation    	s_cur_drv = OP_STOP;

static uint32_t          	s_t_arm = 0;
static uint32_t         	s_t_gap = 0;
static uint8_t 				s_last_eq_color = 0xFF;  // 마지막으로 처리한 "좌/우 동일색". 0xFF = none

// ====== 유틸 ======
static inline uint32_t ms_now(void) { return HAL_GetTick(); }

static inline void drive_if_changed(StepOperation op)
{
    if (op != s_cur_drv)
    {
        step_drive(op);
        s_cur_drv = op;
    }
}

static void seq_clear(void)
{
    s_len = 0;
    s_idx = 0;
}

static bool seq_push(card_prog_op_t op, uint32_t steps)
{
    if (s_len >= CARD_PROG_MAX_LEN) return false;
    s_buf[s_len].op = op;
    s_buf[s_len].target_steps = steps;
    s_len++;
    return true;
}

static const char* op_str(card_prog_op_t op)
{
    switch (op)
    {
        case CPOP_FORWARD:     return "FORWARD";
        case CPOP_REVERSE:     return "REVERSE";
        case CPOP_TURN_RIGHT:  return "TURN_RIGHT";
        case CPOP_TURN_LEFT:   return "TURN_LEFT";
        default:               return "NONE";
    }
}

static inline card_prog_op_t color_to_op(color_t c)
{
    switch (c)
    {
        case COLOR_GREEN:  return CPOP_FORWARD;
        case COLOR_RED:    return CPOP_REVERSE;
        case COLOR_YELLOW: return CPOP_TURN_RIGHT;
        case COLOR_BLUE:   return CPOP_TURN_LEFT;
        default:           return CPOP_NONE;
    }
}

static inline uint32_t color_to_steps(color_t c)
{
    switch (c)
    {
        case COLOR_GREEN:  return CARD_PROG_STEPS_FORWARD;
        case COLOR_RED:    return CARD_PROG_STEPS_BACKWARD;
        case COLOR_YELLOW: return CARD_PROG_STEPS_TURN_RIGHT;
        case COLOR_BLUE:   return CARD_PROG_STEPS_TURN_LEFT;
        default:           return 0;
    }
}

static inline StepOperation op_to_drv(card_prog_op_t op)
{
    switch (op)
    {
        case CPOP_FORWARD:     return OP_FORWARD;
        case CPOP_REVERSE:     return OP_REVERSE;
        case CPOP_TURN_RIGHT:  return OP_TURN_RIGHT;
        case CPOP_TURN_LEFT:   return OP_TURN_LEFT;
        default:               return OP_STOP;
    }
}

static void start_current_item(void)
{
    // 오도메트리 0으로 시작
    odometry_steps_init();
    step_stop();
    step_set_hold(HOLD_BRAKE);

    StepOperation drv = op_to_drv(s_buf[s_idx].op);
    drive_if_changed(drv);

    uart_printf("[CARD-PROG] start #%u/%u op=%s target=%lu\r\n",
                (unsigned)(s_idx + 1), (unsigned)s_len,
                op_str(s_buf[s_idx].op),
                (unsigned long)s_buf[s_idx].target_steps);

    s_state = CARD_PROG_RUNNING;
}

// ====== 공개 API ======
void card_prog_init(void)
{
    s_state = CARD_PROG_IDLE;
    s_mode  = MODE_INVALID;
    s_cur_drv = OP_STOP;
    s_last_eq_color = 0xFF;

    seq_clear();

    step_drive(OP_STOP);
    step_set_hold(HOLD_BRAKE);

    uart_printf("[CARD-PROG] init\r\n");
}

void card_prog_set_mode(mode_sw_t m)
{
    s_mode = m;

    if (s_mode != MODE_CARD)
    {
        // 모드 이탈 → 즉시 정지
        card_prog_stop();
        s_state = CARD_PROG_IDLE;
        s_last_eq_color = 0xFF;  // 모드 이탈 시 엣지 상태 리셋
    }
    else
    {
        if (s_state == CARD_PROG_IDLE)
            s_state = CARD_PROG_PAUSED; // 준비상태(버퍼 있으면 GO로 실행)
    }
}

void card_prog_clear(void)
{
    seq_clear();
    card_prog_stop();
    s_state = (s_mode == MODE_CARD) ? CARD_PROG_PAUSED : CARD_PROG_IDLE;
    s_last_eq_color = 0xFF;
    uart_printf("[CARD-PROG] cleared\r\n");
}

void card_prog_stop(void)
{
    drive_if_changed(OP_STOP);
    step_set_hold(HOLD_BRAKE);
    if (s_state != CARD_PROG_IDLE)
        s_state = CARD_PROG_PAUSED; // 버퍼 보존
//    uart_printf("[CARD-PROG] stop\r\n");
}

void card_prog_start(void)
{
    if (s_mode != MODE_CARD || s_len == 0)
    {
        uart_printf("[CARD-PROG] start ignored (mode=%d, len=%u)\r\n",
                    (int)s_mode, s_len);
        return;
    }

    s_idx   = 0;
    s_state = CARD_PROG_ARMED;
    s_t_arm = ms_now();
    drive_if_changed(OP_STOP);

    uart_printf("[CARD-PROG] GO -> arm %ums, len=%u\r\n",
                (unsigned)CARD_PROG_ARM_DELAY_MS, s_len);
}

// 버튼: GO/RESUME/DELETE만 사용
void card_prog_on_button(btn_id_t id)
{
    if (s_mode != MODE_CARD) return;

    switch (id)
    {
        case BTN_EXECUTE: card_prog_start(); break;
        case BTN_RESUME:  card_prog_stop();  break;
        case BTN_DELETE:  card_prog_clear(); break;
        default:          break;
    }
}

// 좌/우 동일 색 입력 → enqueue(or repeat)
void card_prog_on_dual_equal(uint8_t left, uint8_t right)
{
    if (s_mode != MODE_CARD) return;
    if (left >= COLOR_COUNT || right >= COLOR_COUNT) return;
    if (left != right) return;          // 좌/우 동일하지 않으면 무시

    uint8_t c = left;

    // ★ 같은 동일색이 계속 유지되는 동안은 무시
//    if (c == s_last_eq_color)		//잠깐 없앰
//        return;

    // ★ 여기서부터가 "새로운 동일색" 엣지
    s_last_eq_color = c;

    color_t col = (color_t)c;
    app_rgb_actions_notify_card_color(col);   // <-- rgb 색깔 전달

    // 반복 카드: PINK(+1), PURPLE(+2), LIGHT_GREEN(+3)
    uint8_t rep = 0;
    if (col == COLOR_PINK)             rep = 1;
    else if (col == COLOR_PURPLE)      rep = 2;
    else if (col == COLOR_WHITE)	   rep = 3;

    if (rep > 0)
    {
        if (s_len == 0) return; // 직전 동작 없으면 무시
        card_item_t last = s_buf[s_len - 1];
        uint8_t added = 0;
        while (rep-- && s_len < CARD_PROG_MAX_LEN)
        {
            s_buf[s_len++] = last;
            added++;
        }
        if (added)
            uart_printf("[CARD-PROG] repeat +%u of %s (len=%u)\r\n",
                        added, op_str(last.op), s_len);
        return;
    }

    // 일반 동작 색(G/R/Y/B)
    card_prog_op_t op  = color_to_op(col);
    uint32_t       st  = color_to_steps(col);
    if (op == CPOP_NONE || st == 0) return;

    if (seq_push(op, st))
        uart_printf("[CARD-PROG] push %s (len=%u)\r\n", op_str(op), s_len);
    else
        uart_printf("[CARD-PROG] buffer full (%u)\r\n", CARD_PROG_MAX_LEN);
}

void card_prog_service(void)
{
    if (s_mode != MODE_CARD)
    {
        card_prog_stop();
        return;
    }

    switch (s_state)
    {
        case CARD_PROG_IDLE:
        case CARD_PROG_PAUSED:
        default:
        {
            drive_if_changed(OP_STOP);
            break;
        }

        case CARD_PROG_ARMED:
        {
        	if ((ms_now() - s_t_arm) >= CARD_PROG_ARM_DELAY_MS)
			{
				// 첫 아이템 시작: 오도메트리 리셋 후 구동
				odometry_steps_init();                    // ★ 0으로 리셋
				drive_if_changed(s_buf[s_idx].op);
				uart_printf("[SEQ] start idx=%u/%u op=%d target=%lu\r\n",
							s_idx + 1, s_len, (int)s_buf[s_idx].op,
							(unsigned long)s_buf[s_idx].target_steps);

				s_state = CARD_PROG_RUNNING;
			}
            else
            {
                drive_if_changed(OP_STOP);
                rgb_set_color(RGB_ZONE_EYES, COLOR_BLACK);
				rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
            }
            break;
        }

        case CARD_PROG_RUNNING:
        {
            int32_t exec = get_executed_steps();
            uint32_t abs_exec = (exec < 0) ? (uint32_t)(-exec) : (uint32_t)exec;

            if (abs_exec >= s_buf[s_idx].target_steps)
            {
                // 아이템 완료
                if ((s_idx + 1) >= s_len)
                {
                    // 마지막 → 종료(버퍼 보존)
                    s_state = CARD_PROG_PAUSED;
                    drive_if_changed(OP_STOP);
                    uart_printf("[CARD-PROG] done. buffer kept (len=%u)\r\n", s_len);
                    rgb_set_color(RGB_ZONE_EYES, COLOR_YELLOW);
					rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_YELLOW);
                }
                else
                {
                    // 다음 아이템 전 GAP 진입
                    s_idx++;
                    s_state = CARD_PROG_GAP;
                    s_t_gap = ms_now();
                    drive_if_changed(OP_STOP);
                    uart_printf("[CARD-PROG] gap %ums before #%u/%u\r\n",
                                (unsigned)CARD_PROG_INTER_GAP_MS,
                                (unsigned)(s_idx + 1), (unsigned)s_len);
                }
            }
            break;
        }

        case CARD_PROG_GAP:
        {
            if ((ms_now() - s_t_gap) >= CARD_PROG_INTER_GAP_MS)
            {
                start_current_item();
            }
            else
            {
                drive_if_changed(OP_STOP);
                if((ms_now() - s_t_gap) <= 500)
				{
					rgb_set_color(RGB_ZONE_EYES, COLOR_WHITE);
					rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_WHITE);
				}
				else
				{
					rgb_set_color(RGB_ZONE_EYES, COLOR_BLACK);
					rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
				}
            }
            break;
        }
    }
}

card_prog_state_t card_prog_get_state(void)
{
    return s_state;
}

uint8_t card_prog_get_len(void)
{
    return s_len;
}

uint8_t card_prog_get_index(void)
{
    return s_idx;
}
