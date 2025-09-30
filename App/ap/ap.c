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
extern TIM_HandleTypeDef htim16;

static void apply_mode_button_mask(mode_sw_t m, bool calib_active);




void ap_init(void)
{
	i2c_init();
	uart_init();

	buzzer_init_pwm();

	led_init();
	rgb_init();

	color_init();
	color_calib_init();

	lp_stby_init();
	mode_sw_init();

	btn_init();
	btn_prog_init();
	btn_action_init();

	card_prog_init();
	card_action_init();

	lt_prog_init(NULL);
	line_tracing_enable(false);

	step_init_all();
    // [PATCH] 부팅 직후 확실히 정지 1회
//	s_current_op = OP_STOP;
//    step_drive(s_current_op);

	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_TIM_Base_Start_IT(&htim16);

	load_color_reference_table();
	calculate_color_brightness_offset();
	debug_print_color_reference_table();

	buzzer_play_pororororong();
}



void ap_main(void)
{
	mode_sw_t cur_mode = mode_sw_get();
	bool prev_calib_active = color_calib_is_active();

	apply_mode_button_mask(cur_mode, prev_calib_active);

	while(1)
	{
		// --- 모드 변경 처리 ---
        mode_sw_t m;
        cur_mode = mode_sw_get();          // 재확인
        if(cur_mode == MODE_CARD)
        {
        	card_prog_set_mode(cur_mode);
        }

        if (mode_sw_changed(&m) || cur_mode == MODE_INVALID)
        {
            cur_mode = mode_sw_get();          // 재확인
            apply_mode_button_mask(cur_mode, color_calib_is_active());
            uart_printf("[MODE] %s\r\n", mode_sw_name(cur_mode));
            // ★ 여기서만 한 번
//			card_prog_set_mode(cur_mode);
        }

        // --- 캘리 상태 변화 처리 ---
        bool now_calib_active = color_calib_is_active();
        if (now_calib_active != prev_calib_active)
        {
            prev_calib_active = now_calib_active;
            apply_mode_button_mask(cur_mode, now_calib_active);
            uart_printf("[CAL] %s -> mask re-applied\r\n",
                        now_calib_active ? "ACTIVE" : "IDLE");
        }

        // --- (옵션) 라인트레이싱에서 3초 길게 FORWARD → 캘리 진입 ---
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

        // --- 카드 모드에 센서 피드 (양쪽 동일일 때만 큐잉) ---
		if (cur_mode == MODE_CARD && !now_calib_active)
		{
			uint8_t left  = classify_color_side(BH1749_ADDR_LEFT);
			uint8_t right = classify_color_side(BH1749_ADDR_RIGHT);
			if(btn_pop_long_press(BTN_FORWARD, 200))
			{
				card_prog_on_dual_equal(left, right);   // 동일 색만 enqueue/반복 처리
			}
//			card_prog_service();
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
						case BTN_FORWARD:
						case BTN_BACKWARD:
						case BTN_LEFT:
						case BTN_RIGHT:
							btn_prog_on_button(pressed);  // ★ 딱 1회 실행
							break;

						case BTN_EXECUTE:
						case BTN_DELETE:
						case BTN_RESUME:
							btn_prog_on_button(pressed);         // 단발 모드에선 STOP로 처리
							break;

						default:
							break;
					}
				}
				else if (cur_mode == MODE_CARD)
				{
					// ★ 카드 큐: 실행 제어만 버튼으로 받는다
					switch (pressed)
					{
						case BTN_EXECUTE:
						case BTN_RESUME:
						case BTN_DELETE:
							card_prog_on_button(pressed);
							break;
						default:
							// 카드 큐의 전/후/좌/우 입력은 "센서"로만 받음
							break;
					}
				}
				else if (cur_mode == MODE_LINE_TRACING)
				{
					switch (pressed)
					{
						case BTN_EXECUTE:
						case BTN_RESUME:
						case BTN_DELETE:
							lt_prog_on_button(pressed);
							break;
						default:
							// 카드 큐의 전/후/좌/우 입력은 "센서"로만 받음
							break;
					}
				}

			}
		}

		if (!color_calib_is_active() && cur_mode == MODE_BUTTON)
		{
		    btn_prog_service(cur_mode, false);
		}

		if (!color_calib_is_active() && cur_mode == MODE_CARD)
		{
			// 버튼 모드가 아니거나 캘리 중이면 내부에서 STOP+PAUSE 처리됨
			card_prog_service();
		}

		if (!color_calib_is_active() && cur_mode == MODE_LINE_TRACING)
		{
			// 버튼 모드가 아니거나 캘리 중이면 내부에서 STOP+PAUSE 처리됨
			lt_prog_service(tim6_get_ms());
		}
		else
		{
			step_set_period_ticks(500, 500);//라인 트레이싱 끝나고 속도 원상 복귀
		}

		app_rgb_actions_notify_press(pressed);
		app_rgb_actions_poll(cur_mode);
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
			btn_enable_mask_set(BTN_MASK_CARD);		//임시방편으로 card도 forward 버튼 입력해야 되게끔 설정
			break;
	}
}
