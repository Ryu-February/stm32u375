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

	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_Base_Start_IT(&htim6);
}



void ap_main(void)
{
	mode_sw_t cur_mode = MODE_INVALID;

	while(1)
	{
        mode_sw_t m;
        if (mode_sw_changed(&m) || cur_mode == MODE_INVALID)
        {
            cur_mode = mode_sw_get();          // 재확인
//            apply_mode_button_mask(cur_mode, (s_calib == CALIB_ACTIVE));
            uart_printf("[MODE] %s\r\n", mode_sw_name(cur_mode));
        }


        /* 2) LINE_TRACING에서 FORWARD 3초 길게 → 캘리 진입 */
        if (cur_mode == MODE_LINE_TRACING && !color_calib_is_active())
        {
            if (btn_pop_long_press(BTN_FORWARD, 3000))   // 3000ms
            {
                color_calib_enter();
                apply_mode_button_mask(cur_mode, true);  // 캘리 중 버튼 제한(선택)
                // 현재 타깃 안내
                int idx = color_calib_index();
                int tot = color_calib_total();
                color_t tgt = color_calib_current_target();
                uart_printf("[CAL] enter %d/%d, target=%d\r\n", idx + 1, tot, (int)tgt);
            }
        }

        btn_id_t pressed;
		StepOperation op; // ★ 모드 변경 감지 → 버튼 마스크 갱신
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
				// 평상시 동작
				if (cur_mode == MODE_BUTTON)
				{
					// 버튼 모드에서만 7개 조작 반영
					switch (pressed)
					{
						case BTN_GO:       op = OP_STOP;        break;
						case BTN_FORWARD:  op = OP_FORWARD;     break;
						case BTN_BACKWARD: op = OP_REVERSE;     break;
						case BTN_LEFT:     op = OP_TURN_LEFT;   break;
						case BTN_RIGHT:    op = OP_TURN_RIGHT;  break;
						case BTN_DELETE:
						case BTN_RESUME:
						default:           break;
					}
				}
				else if (cur_mode == MODE_LINE_TRACING)
				{
					// LINE_TRACING 모드에서의 단추 의미를 정의하고 싶으면 여기서 처리
					// 예) GO로 긴급정지, FORWARD 짧은 클릭은 무시(캘리 long 전용)
					if (pressed == BTN_GO)
						op = OP_STOP;
				}
				else
				{
					// CARD 모드: 필요 시 GO/DELETE/RESUME만 처리
					if (pressed == BTN_GO)
						op = OP_STOP;
				}

				if (op != OP_NONE)
					step_drive(op);
				app_rgb_actions_notify_press(pressed);
				app_rgb_actions_poll();
			}
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
