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

#endif /* INPUT_BTN_H_ */
