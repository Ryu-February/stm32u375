/*
 * btn_prog.c
 *
 *  Created on: Sep 26, 2025
 *      Author: RCY
 */


#include "btn_prog.h"
#include "uart.h"

// 단일 작성자 원칙: 이 모듈이 step_drive()를 직접 호출한다.
// ap.c에서는 step_drive()를 다시 호출하지 말 것.

typedef struct
{
    StepOperation op;
    uint32_t      target_steps;   // 이 아이템의 목표 스텝(절대값)
} seq_item_t;

// ===== 내부 상태 =====
static seq_item_t       s_buf[BTN_PROG_MAX_LEN];
static uint8_t          s_len;
static uint8_t          s_idx;            // 현재 실행 인덱스
static btn_prog_state_t s_state;

static uint32_t         s_t_arm;          // ARMED 시작 시각(ms)
static StepOperation    s_cur_op;         // 마지막으로 보낸 모터 명령(래치)

// ===== 유틸 =====
static inline uint32_t ms_now(void)
{
    return HAL_GetTick();
}

static inline void drive_if_changed(StepOperation op)
{
    if (op != s_cur_op)
    {
        step_drive(op);
        s_cur_op = op;
    }
}

static bool enqueue(StepOperation op, uint32_t steps)
{
    if (s_len >= BTN_PROG_MAX_LEN)
        return false;

    s_buf[s_len].op           = op;
    s_buf[s_len].target_steps = steps;
    s_len++;
    return true;
}

static void stop_and_pause(void)
{
    s_state = BTN_PROG_PAUSED;   // 버퍼 보존
    drive_if_changed(OP_STOP);
}

// ===== 공개 API =====
void btn_prog_init(void)
{
    s_len    = 0;
    s_idx    = 0;
    s_state  = BTN_PROG_IDLE;
    s_cur_op = OP_STOP;
}

void btn_prog_clear(void)
{
    s_len    = 0;
    s_idx    = 0;
    s_state  = BTN_PROG_IDLE;
    drive_if_changed(OP_STOP);
}

void btn_prog_on_button(btn_id_t id)
{
    switch (id)
    {
        // ---- 시퀀스에 동작 추가 ----
        case BTN_FORWARD:
        {
            if (enqueue(OP_FORWARD, BTN_PROG_STEPS_FORWARD))
                uart_printf("[SEQ] +FORWARD(%lu) (len=%u)\r\n",
                            (unsigned long)BTN_PROG_STEPS_FORWARD, s_len);
            else
                uart_printf("[SEQ] buffer full!\r\n");
            break;
        }
        case BTN_BACKWARD:
        {
            if (enqueue(OP_REVERSE, BTN_PROG_STEPS_BACKWARD))
                uart_printf("[SEQ] +BACKWARD(%lu) (len=%u)\r\n",
                            (unsigned long)BTN_PROG_STEPS_BACKWARD, s_len);
            else
                uart_printf("[SEQ] buffer full!\r\n");
            break;
        }
        case BTN_LEFT:
        {
            if (enqueue(OP_TURN_LEFT, BTN_PROG_STEPS_TURN_LEFT))
                uart_printf("[SEQ] +LEFT(%lu) (len=%u)\r\n",
                            (unsigned long)BTN_PROG_STEPS_TURN_LEFT, s_len);
            else
                uart_printf("[SEQ] buffer full!\r\n");
            break;
        }
        case BTN_RIGHT:
        {
            if (enqueue(OP_TURN_RIGHT, BTN_PROG_STEPS_TURN_RIGHT))
                uart_printf("[SEQ] +RIGHT(%lu) (len=%u)\r\n",
                            (unsigned long)BTN_PROG_STEPS_TURN_RIGHT, s_len);
            else
                uart_printf("[SEQ] buffer full!\r\n");
            break;
        }

        // ---- 실행/정지/삭제 ----
        case BTN_GO:
        {
            if (s_len == 0)
            {
                uart_printf("[SEQ] GO ignored (empty)\r\n");
                break;
            }
            s_idx   = 0;
            s_state = BTN_PROG_ARMED;
            s_t_arm = ms_now();
            uart_printf("[SEQ] GO -> arm %ums, len=%u\r\n",
                        BTN_PROG_ARM_DELAY_MS, s_len);
            break;
        }
        case BTN_RESUME:
        {
            // ‘멈춤’ 용도: 즉시 정지(버퍼 보존)
            stop_and_pause();
            uart_printf("[SEQ] paused (STOP). buffer kept (len=%u)\r\n", s_len);
            break;
        }
        case BTN_DELETE:
        {
            btn_prog_clear();
            uart_printf("[SEQ] cleared\r\n");
            break;
        }

        default:
        {
            // 무시
            break;
        }
    }
}

void btn_prog_service(mode_sw_t cur_mode, bool calib_active)
{
    // 버튼 모드가 아니거나 캘리 중이면 언제나 정지
    if (cur_mode != MODE_BUTTON || calib_active)
    {
        stop_and_pause();
        return;
    }

    switch (s_state)
    {
        case BTN_PROG_IDLE:
        case BTN_PROG_PAUSED:
        default:
        {
            drive_if_changed(OP_STOP);
            break;
        }

        case BTN_PROG_ARMED:
        {
            if ((ms_now() - s_t_arm) >= BTN_PROG_ARM_DELAY_MS)
            {
                // 첫 아이템 시작: 오도메트리 리셋 후 구동
                odometry_steps_init();                    // ★ 0으로 리셋
                drive_if_changed(s_buf[s_idx].op);
                uart_printf("[SEQ] start idx=%u/%u op=%d target=%lu\r\n",
                            s_idx + 1, s_len, (int)s_buf[s_idx].op,
                            (unsigned long)s_buf[s_idx].target_steps);

                s_state = BTN_PROG_RUNNING;
            }
            else
            {
                drive_if_changed(OP_STOP);
            }
            break;
        }

        case BTN_PROG_RUNNING:
        {
            // 현재 진행 스텝 수 체크 (부호 무시하고 절대값으로 비교)
            int32_t executed = get_executed_steps();     // ★ stepper.c 공개 API
            uint32_t abs_exec = (executed < 0) ? (uint32_t)(-executed) : (uint32_t)executed;

            if (abs_exec >= s_buf[s_idx].target_steps)
            {
                // 다음 아이템으로
                s_idx++;
                if (s_idx >= s_len)
                {
                    // 끝 → 정지(버퍼 보존)
                    s_state = BTN_PROG_IDLE;
                    drive_if_changed(OP_STOP);
                    uart_printf("[SEQ] done. buffer kept (len=%u)\r\n", s_len);
                }
                else
                {
                    // 다음 아이템 시작
                    odometry_steps_init();                // ★ 새 구간 0으로
                    drive_if_changed(s_buf[s_idx].op);
                    uart_printf("[SEQ] idx=%u/%u op=%d target=%lu\r\n",
                                s_idx + 1, s_len, (int)s_buf[s_idx].op,
                                (unsigned long)s_buf[s_idx].target_steps);

                    // (필요하면 여기 사이에 아주 짧은 브레이크 삽입 가능)
                    // step_stop(); delay_ms(10); drive_if_changed(...);
                }
            }
            break;
        }
    }
}

btn_prog_state_t btn_prog_get_state(void)
{
    return s_state;
}

uint8_t btn_prog_get_len(void)
{
    return s_len;
}

uint8_t btn_prog_get_index(void)
{
    return s_idx;
}
