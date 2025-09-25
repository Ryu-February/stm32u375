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

static void apply_mode_button_mask(mode_sw_t m);

void ap_init(void)
{
	i2c_init();
	uart_init();

	led_init();
	rgb_init();

	color_init();

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
            apply_mode_button_mask(cur_mode);
            uart_printf("[MODE] %s\r\n", mode_sw_name(cur_mode));
        }

        btn_id_t pressed;
		StepOperation op; // ★ 모드 변경 감지 → 버튼 마스크 갱신
		if(btn_pop_any_press(&pressed))
		{
			btn_print_one(pressed);                // ← 출력 (비파괴 아님: 이미 pop 했으니 그냥 찍기만)
			app_rgb_actions_notify_press(pressed);
			app_rgb_actions_poll();

			switch (pressed)
			{
				case BTN_GO:
					op = OP_STOP;
					break;
				case BTN_FORWARD:
					op = OP_FORWARD;
					break;
				case BTN_BACKWARD:
					op = OP_REVERSE;
					break;
				case BTN_LEFT:
					op = OP_TURN_LEFT;
					break;
				case BTN_RIGHT:
					op = OP_TURN_RIGHT;
					break;
				default:
					break;

			}
		}
		step_drive(op);


		classify_color_side(BH1749_ADDR_LEFT);
		classify_color_side(BH1749_ADDR_RIGHT);
//		delay_ms(120);
	}
}


static void apply_mode_button_mask(mode_sw_t m)
{
    switch (m)
    {
        case MODE_BUTTON:
            btn_enable_mask_set(BTN_MASK_ALL);     // 7개 전부 활성
            break;

        case MODE_CARD:
        case MODE_LINE_TRACING:
        default:
            btn_enable_mask_set(BTN_MASK_BASE3);   // GO/DELETE/RESUME만 활성
            break;
    }
}

