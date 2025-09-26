/*
 * ap.h
 *
 *  Created on: Sep 19, 2025
 *      Author: RCY
 */

#ifndef AP_AP_H_
#define AP_AP_H_



#include "utils.h"

#include "i2c.h"
#include "uart.h"

#include "led.h"
#include "rgb.h"
#include "btn.h"
#include "color.h"
#include "calib.h"
#include "flash.h"
#include "mode_sw.h"
#include "btn_prog.h"
#include "btn_action.h"
#include "rgb_actions.h"


#include "lp_stby.h"
#include "stepper.h"




void ap_init(void);
void ap_main(void);



#endif /* AP_AP_H_ */
