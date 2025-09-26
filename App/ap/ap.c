/*
 * ap.c
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */



#include "ap.h"


extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim6;

static void apply_mode_button_mask(mode_sw_t m, bool calib_active);

// [PATCH] 마지막으로 보낸 모터 명령 기억
static StepOperation s_current_op = OP_STOP;   // 래치된 현재 동작

void ap_init(void)
{
	i2c_init();
	uart_init();

	led_init();
	rgb_init();

	color_init();
	color_calib_init();

	lp_stby_init();
	mode_sw_init();

	step_init_all();
    // [PATCH] 부팅 직후 확실히 정지 1회
	s_current_op = OP_STOP;
    step_drive(s_current_op);

	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_Base_Start_IT(&htim6);

	load_color_reference_table();
	calculate_color_brightness_offset();
	debug_print_color_reference_table();
}



void ap_main(void)
{
	mode_sw_t cur_mode = mode_sw_get();
	bool prev_calib_active = color_calib_is_active();

	apply_mode_button_mask(cur_mode, prev_calib_active);

	while(1)
	{
		StepOperation op_req = s_current_op;

        mode_sw_t m;
        if (mode_sw_changed(&m) || cur_mode == MODE_INVALID)
        {
            cur_mode = mode_sw_get();          // 재확인
            apply_mode_button_mask(cur_mode, color_calib_is_active());
            uart_printf("[MODE] %s\r\n", mode_sw_name(cur_mode));
        }

        // 2) ★ 캘리 상태 변화 반영(핵심)
        bool now_calib_active = color_calib_is_active();
        if (now_calib_active != prev_calib_active)
        {
            prev_calib_active = now_calib_active;
            apply_mode_button_mask(cur_mode, now_calib_active);
            uart_printf("[CAL] %s -> mask re-applied\r\n",
                        now_calib_active ? "ACTIVE" : "IDLE");
        }

        /* 2) LINE_TRACING에서 FORWARD 3초 길게 → 캘리 진입 */
        if (cur_mode == MODE_LINE_TRACING && !color_calib_is_active())
        {
            if (btn_pop_long_press(BTN_FORWARD, 3000))   // 3000ms
            {
                color_calib_enter();
                apply_mode_button_mask(cur_mode, true);  // 캘리 중 버튼 제한(선택)
                flash_erase_color_table(BH1749_ADDR_LEFT);
				flash_erase_color_table(BH1749_ADDR_RIGHT);
                // 현재 타깃 안내
                int idx = color_calib_index();
                int tot = color_calib_total();
                color_t tgt = color_calib_current_target();
                uart_printf("[CAL] enter %d/%d, target=%d\r\n", idx + 1, tot, (int)tgt);
            }
        }

        btn_id_t pressed;
        // [PATCH] 루프 기본값: 항상 STOP
		if(btn_pop_any_press(&pressed))
		{
			// 캘리브레이션 진행 중
			if (color_calib_is_active())
			{

				// FORWARD 딸깍 → 한 단계 진행
				if (pressed == BTN_FORWARD)
				{
					color_calib_on_forward_click();

					// 끝났다면 마스크 원복
					if (!color_calib_is_active())
						apply_mode_button_mask(cur_mode, false);
				}
				// 캘리 중엔 나머지는 마스크로 차단되어 여기 안 옴
			}
			else
			{
				btn_print_one(pressed);                // ← 출력 (비파괴 아님: 이미 pop 했으니 그냥 찍기만)
				// 평상시 동작
				if (cur_mode == MODE_BUTTON)
				{
					// 버튼 모드에서만 7개 조작 반영
					switch (pressed)
					{
						case BTN_GO:       op_req = OP_STOP;        break;
						case BTN_FORWARD:  op_req = OP_FORWARD;     break;
						case BTN_BACKWARD: op_req = OP_REVERSE;     break;
						case BTN_LEFT:     op_req = OP_TURN_LEFT;   break;
						case BTN_RIGHT:    op_req = OP_TURN_RIGHT;  break;
						case BTN_DELETE:
						case BTN_RESUME:
						default:           break;
					}
				}
				else if (cur_mode == MODE_LINE_TRACING)
				{
					// 필요 시 GO만 정지로 해석 (기본은 이미 STOP이므로 생략 가능)
					if (pressed == BTN_GO) op_req = OP_STOP;
				}
				else /* MODE_CARD */
				{
					if (pressed == BTN_GO) op_req = OP_STOP;
				}

				app_rgb_actions_notify_press(pressed);
				app_rgb_actions_poll();
			}
		}
		// 캘리브레이션 활성 시엔 무조건 STOP 유지
		if (color_calib_is_active())
			op_req = OP_STOP;

		if(cur_mode != MODE_BUTTON)
			op_req = OP_STOP;
		// [PATCH] 변경이 있을 때만 모터에 명령
		if (op_req != s_current_op)
		{
			step_drive(op_req);
			s_current_op = op_req;
		}

		classify_color_side(BH1749_ADDR_LEFT);
		classify_color_side(BH1749_ADDR_RIGHT);
//		delay_ms(120);
	}
}


static void apply_mode_button_mask(mode_sw_t m, bool calib_active)
{
	// 캘리브레이션 중에는 FORWARD만 쓰고 싶다면 여기에서 별도 마스크로 잠글 수도 있음
	if (calib_active)
	{
		btn_enable_mask_set( BTN_BIT(BTN_FORWARD) ); // 선택: 캘리브레이션 중 포워드만 허용
		return;
	}

	switch (m)
	{
		case MODE_BUTTON:
			btn_enable_mask_set(BTN_MASK_ALL);
			break;

		case MODE_LINE_TRACING:
			btn_enable_mask_set(BTN_MASK_LINE);      // ★ GO/DELETE/RESUME + FORWARD
			break;

		case MODE_CARD:
		default:
			btn_enable_mask_set(BTN_MASK_BASE3);
			break;
	}
}
