/*
 * line_tracing.h
 *
 *  Created on: Sep 17, 2025
 *      Author: RCY
 */

#ifndef MOTION_LINE_TRACING_H_
#define MOTION_LINE_TRACING_H_


#include "def.h"
#include "color.h"




typedef struct
{
    float    Kp;
    float    Ki;
    float    Kd;
    uint16_t base_ticks;   // 예: 1500
    uint16_t min_ticks;    // 예: 500
    uint16_t max_ticks;    // 예: 2500
    uint8_t  interval_ms;  // PID 갱신 간격(예: 5ms)
} lt_config_t;




void line_tracing_init(const lt_config_t *cfg);
void line_tracing_enable(bool on);
bool line_tracing_enabled(void);

void line_tracing_set_offset(uint8_t side, uint16_t avg);
void line_tracing_set_gains(float kp, float ki, float kd);

void line_tracing_update(uint32_t now_ms);


#endif /* MOTION_LINE_TRACING_H_ */
