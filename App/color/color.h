/*
 * color.h
 *
 *  Created on: Sep 25, 2025
 *      Author: RCY
 */

#ifndef COLOR_COLOR_H_
#define COLOR_COLOR_H_


#include "def.h"
#include "rgb.h"

// ==== Device I2C address (7-bit) ====
#define BH1749_ADDR_LEFT          0x38
#define BH1749_ADDR_RIGHT         0x39

// ==== Register Addresses (BH1749NUC) ====
#define BH1749_REG_SYSTEM_CONTROL 0x40
#define BH1749_REG_MODE_CTRL1     0x41
#define BH1749_REG_MODE_CTRL2     0x42
#define BH1749_REG_RED_LSB        0x50
#define BH1749_REG_GREEN_LSB      0x52
#define BH1749_REG_BLUE_LSB       0x54
#define BH1749_REG_IR_LSB         0x58
#define BH1749_REG_GREEN2_LSB     0x5A

// SYSTEM_CONTROL (0x40)
#define BH1749_SW_RESET           (1u << 7)
#define BH1749_INT_RESET          (1u << 6)
// PART ID는 0x40 하위 6비트 읽기 전용(0x0D) — 필요시 확인.

// MODE_CONTROL1 (0x41)
// [IR_GAIN1:0][RGB_GAIN1:0][MEAS_MODE2:0]
// IR/RGB gain: 01 = x1, 11 = x32
// MEAS_MODE: 101=35ms, 010=120ms, 011=240ms
#define BH1749_GAIN_X1            0x01
#define BH1749_GAIN_X32           0x03
#define BH1749_MEAS_35MS          0x05
#define BH1749_MEAS_120MS         0x02
#define BH1749_MEAS_240MS         0x03

// MODE_CONTROL2 (0x42)
#define BH1749_RGB_EN             (1u << 4)
#define BH1749_VALID              (1u << 7)  // Data ready flag (읽기용 의미)

// ==== App limits ====
#define MAX_INSERTED_COMMANDS     20

typedef struct
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t ir;     // BH1749: CLEAR 대신 IR 채널
} bh1749_color_data_t;

typedef struct
{
    uint16_t red_raw;
    uint16_t green_raw;
    uint16_t blue_raw;
} rgb_raw_t;

typedef struct
{
    rgb_raw_t raw;
    color_t   color;
    uint8_t   _pad[1];
    uint64_t  offset;     // 밝기 보정/플래시에 저장용
} reference_entry_t;

typedef enum
{
    MODE_NONE = 0,
    MODE_FORWARD,
    MODE_BACKWARD,
    MODE_LEFT,
    MODE_RIGHT,
    MODE_REPEAT_ONCE,
    MODE_REPEAT_TWICE,
    MODE_REPEAT_THIRD,
    COLOR_MODE_COUNT
} color_mode_t;

// ==== BH1749 low-level ====
void     bh1749_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data);
uint8_t  bh1749_read_u8(uint8_t dev_addr, uint8_t reg);
uint16_t bh1749_read_u16(uint8_t dev_addr, uint8_t lsb_reg);
void     bh1749_init(uint8_t dev_addr, uint8_t rgb_gain, uint8_t ir_gain, uint8_t meas_mode);

// ==== High-level color ====
void                color_init(void);
bh1749_color_data_t bh1749_read_rgbir(uint8_t dev_addr);
void                save_color_reference(uint8_t sensor_side, color_t color, uint16_t r, uint16_t g, uint16_t b);
color_t             classify_color(uint8_t left_right, uint16_t r, uint16_t g, uint16_t b, uint16_t ir);
uint8_t             classify_color_side(uint8_t color_side);

const char*         color_to_string(color_t color);
void                load_color_reference_table(void);
void                debug_print_color_reference_table(void);
uint32_t            calculate_brightness(uint16_t r, uint16_t g, uint16_t b);
void                calculate_color_brightness_offset(void);
color_mode_t        color_to_mode(color_t color);


#endif /* COLOR_COLOR_H_ */
