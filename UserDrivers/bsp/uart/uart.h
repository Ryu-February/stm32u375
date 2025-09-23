/*
 * uart.h
 *
 *  Created on: Sep 9, 2025
 *      Author: RCY
 */

#ifndef BSP_UART_UART_H_
#define BSP_UART_UART_H_




#include "def.h"


void uart_init(void);
void uart_printf(const char *fmt, ...);


#endif /* BSP_UART_UART_H_ */
