/*
 * btn.h
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */

#ifndef INPUT_BTN_H_
#define INPUT_BTN_H_


#include "def.h"


#define BTN_DEBOUNCE_MS		20

// ---- 모드별 버튼 마스크 ----
#define BTN_BIT(id)           (1u << (id))
#define BTN_MASK_ALL   ( BTN_BIT(BTN_GO) | BTN_BIT(BTN_DELETE) | BTN_BIT(BTN_RESUME) | \
                         BTN_BIT(BTN_FORWARD) | BTN_BIT(BTN_BACKWARD) | \
                         BTN_BIT(BTN_LEFT) | BTN_BIT(BTN_RIGHT) )
#define BTN_MASK_BASE3 ( BTN_BIT(BTN_GO) | BTN_BIT(BTN_DELETE) | BTN_BIT(BTN_RESUME) )

typedef enum
{
	BTN_GO = 0,
	BTN_DELETE,
	BTN_RESUME,
	BTN_FORWARD,
	BTN_BACKWARD,
	BTN_LEFT,
	BTN_RIGHT,
	BTN_COUNT
}btn_id_t;





void btn_init(void);
void btn_update_1ms(void);
bool btn_is_pressed(btn_id_t id);
bool btn_get_press(btn_id_t id);
bool btn_pop_any_press(btn_id_t *out_id);

void btn_print_states(void);
void btn_print_events(void);
void btn_print_one(btn_id_t id);

// ▲ 활성화 마스크 API 추가
void btn_enable_mask_set(uint32_t mask);
uint32_t btn_enable_mask_get(void);


#endif /* INPUT_BTN_H_ */
