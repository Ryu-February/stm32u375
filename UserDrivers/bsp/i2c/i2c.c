/*
 * i2c.c (STM32U375RGT6)
 *  Bare-metal I2C1 @ PB8(SCL)/PB9(SDA), 100 kHz (HSI16 kernel clock)
 */

#include "i2c.h"

#define I2C_TIMEOUT_US    (2000U)   // 간단한 폴링 타임아웃 (필요시 조정)

// HSI16(16 MHz) 기반 100 kHz TIMINGR (표준모드)
// CubeMX에서 흔히 나오는 값. 클럭 변경 시 반드시 재계산!
#define I2C_TIMINGR_100K_HSI16   (0x00303D5BUL)

static inline void busy_wait(volatile uint32_t cnt)
{
    while (cnt--) __NOP();
}

void i2c_init(void)
{
    // 0) GPIOB 클럭 Enable (U3: AHB2ENR1)
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOBEN;

    // 1) PB8, PB9 AF4(Open-drain, Pull-up, Very High)
    // MODER: AF(10)
    GPIOB->MODER &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
    GPIOB->MODER |=  ((2U << (8 * 2)) | (2U << (9 * 2)));

    // OTYPER: Open-drain(1)
    GPIOB->OTYPER |= (1U << 8) | (1U << 9);

    // OSPEEDR: Very High(11)
    GPIOB->OSPEEDR &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
    GPIOB->OSPEEDR |=  ((3U << (8 * 2)) | (3U << (9 * 2)));

    // PUPDR: Pull-up(01)
    GPIOB->PUPDR &= ~((3U << (8 * 2)) | (3U << (9 * 2)));
    GPIOB->PUPDR |=  ((1U << (8 * 2)) | (1U << (9 * 2)));

    // AFR[1]: PB8/9 → AF4(I2C1)
    GPIOB->AFR[1] &= ~((0xFU << ((8 - 8) * 4)) | (0xFU << ((9 - 8) * 4)));
    GPIOB->AFR[1] |=  ((4U   << ((8 - 8) * 4)) | (4U   << ((9 - 8) * 4)));

    // 2) HSI16 켜고 준비 대기 (I2C 커널클럭 소스로 사용)
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) { /* wait */ }

    // 3) I2C1 커널클럭 소스 선택: I2C1SEL = 0b10(HSI16)
    //    CCIPR1 레지스터의 I2C1SEL 비트필드 사용 (제품별 비트 위치는 헤더에 정의)
    RCC->CCIPR1 &= ~RCC_CCIPR1_I2C1SEL;
    RCC->CCIPR1 |=  (2U << RCC_CCIPR1_I2C1SEL_Pos);  // 0:PCLK1, 1:SYSCLK, 2:HSI16

    // 4) I2C1 클럭 Enable
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;

    // (선택) I2C1 리셋
    RCC->APB1RSTR1 |=  RCC_APB1RSTR1_I2C1RST;
    RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C1RST;

    // 5) I2C1 Disable 후 타이밍/필터 설정
    I2C1->CR1 &= ~I2C_CR1_PE;

    // 아날로그 필터 ON(기본), 디지털필터 0 클럭
    I2C1->CR1 &= ~I2C_CR1_ANFOFF;
    I2C1->CR1 &= ~I2C_CR1_DNF;

    // TIMINGR 설정 (100 kHz @ HSI16)
    I2C1->TIMINGR = I2C_TIMINGR_100K_HSI16;

    // 6) I2C Enable
    I2C1->CR1 |= I2C_CR1_PE;
}

static bool i2c_wait_flag(volatile uint32_t *reg, uint32_t mask, bool set)
{
    uint32_t t = I2C_TIMEOUT_US;
    while (t--)
    {
        uint32_t v = *reg;
        if (set ? (v & mask) : !(v & mask))
            return true;
        // 대충 1us 정도 기다리기 (코어 클럭에 따라 조절)
        busy_wait(50);
    }
    return false;
}

void i2c_write(uint8_t slave_addr, uint8_t reg_addr, uint8_t data)
{
    // 준비: STOP/ERR 플래그 클리어
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF;

    // CR2를 한 번에 구성: SADD, NBYTES=2, Write, START, AUTOEND
    uint32_t cr2 =
        ((uint32_t)(slave_addr << 1) << I2C_CR2_SADD_Pos) |
        (2U << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_AUTOEND |
        I2C_CR2_START; // RD_WRN=0(Write)
    I2C1->CR2 = cr2;

    // TXIS 대기 후 레지스터 주소
    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_TXIS, true)) return;
    I2C1->TXDR = reg_addr;

    // TXIS 대기 후 데이터
    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_TXIS, true)) return;
    I2C1->TXDR = data;

    // STOPF 대기 및 클리어
    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_STOPF, true)) return;
    I2C1->ICR = I2C_ICR_STOPCF;
}

uint8_t i2c_read(uint8_t slave_addr, uint8_t reg_addr)
{
    uint8_t data = 0;

    // 준비: STOP/ERR 플래그 클리어
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF;

    // 1) Write phase: sub-address(레지스터) 전송 (AUTOEND 없이 START만)
    uint32_t cr2_w =
        ((uint32_t)(slave_addr << 1) << I2C_CR2_SADD_Pos) |
        (1U << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_START; // RD_WRN=0
    I2C1->CR2 = cr2_w;

    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_TXIS, true)) goto done;
    I2C1->TXDR = reg_addr;

    // 전송 완료(TC) 대기
    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_TC, true)) goto done;

    // 2) Read phase: 1바이트 읽기 (AUTOEND로 자동 STOP)
    uint32_t cr2_r =
        ((uint32_t)(slave_addr << 1) << I2C_CR2_SADD_Pos) |
        (1U << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_RD_WRN |
        I2C_CR2_START |
        I2C_CR2_AUTOEND;
    I2C1->CR2 = cr2_r;

    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_RXNE, true)) goto done;
    data = (uint8_t)I2C1->RXDR;

    if (!i2c_wait_flag(&I2C1->ISR, I2C_ISR_STOPF, true)) goto done;
    I2C1->ICR = I2C_ICR_STOPCF;

done:
    return data;
}
