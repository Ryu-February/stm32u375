/*
 * lp_stby.c
 *
 *  Created on: Sep 24, 2025
 *      Author: RCY
 */


#include "lp_stby.h"
#include "buzzer.h"

// ==== 하드웨어 핀 ====
// PB1에 Delete 스위치 (프로젝트 기준)
#define LP_STBY_BTN_PORT     GPIOA
#define LP_STBY_BTN_PIN      GPIO_PIN_0

// 내부 상태
static bool		s_last_pressed   = false;	// 마지막 샘플(raw)
static uint16_t s_stable_ms      = 0;   		// 같은 raw가 지속된 ms
static bool 	s_stable_pressed = false;   	// 디바운스 통과 상태(1: not pressed)
static uint32_t s_press_ms       = 0;   		// 눌림 지속 ms
static bool     s_fired          = false;

static inline bool btn_pressed_raw(void)
{
    // Active-Low: 눌림 → LOW
    return (HAL_GPIO_ReadPin(LP_STBY_BTN_PORT, LP_STBY_BTN_PIN) == GPIO_PIN_RESET);
}

static inline bool was_from_standby(void)
{
#if defined(PWR_WUSR_SBF)
    return (READ_BIT(PWR->WUSR, PWR_WUSR_SBF) != 0U);
#elif defined(PWR_SR1_SBF)
    return (READ_BIT(PWR->SR1, PWR_SR1_SBF) != 0U);
#else
    return false;
#endif
}

static inline void ensure_wkup1_off_at_boot(void)
{
#ifdef PWR_WUCR1_WUPEN1
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
#endif
#ifdef PWR_WUSR_WUF1
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);  // 잔여 Wake flag 정리
#endif
}

static inline void clear_standby_flag(void)
{
#if defined(PWR_WUSR_SBF)
    WRITE_REG(PWR->WUSR, PWR_WUSR_SBF);   // write 1 to clear
#elif defined(PWR_SCR_CSBF)
    SET_BIT(PWR->SCR, PWR_SCR_CSBF);
#endif
}

static void reenter_standby_now(void)
{
    // Standby 재진입: WKUP1은 진입 직전에만 Enable (네가 이미 만든 함수 사용)
    // wkup1_config_for_active_low();  // 소스선택/플래그클리어/Enable
    __disable_irq();
    HAL_SuspendTick();
    HAL_PWR_EnterSTANDBYMode();
    while (1)
    {
    }
}

// 부팅 초기에 가장 먼저 호출
void lp_stby_boot_gate(void)
{
    // WKUP1이 혹시 켜져 있었다면 우선 꺼서 부팅 초기 판정에 간섭 못 하게
	HAL_Delay(1000);//눈 속임용으로 1초 누를 때 켜지게끔 함 (야매임 ㅇㅇ)

	ensure_wkup1_off_at_boot();

    // 콜드부팅이면 바로 통과
    if (!was_from_standby())
    {
        return;
    }

    // Standby 복귀 플래그 정리
    clear_standby_flag();

    // RC 충전/노이즈 여유
    HAL_Delay(1000);//지금 여기 자체 안 들어옴

    // 1초 길게 누름을 *부팅 초기에* 요구
    // 디바운스 포함: 먼저 LP_STBY_DEBOUNCE_MS 동안 연속 눌림 확인 → 그 다음 1s 지속
    uint16_t stable_ms = 0;

    // (A) 디바운스 구간
    while (stable_ms < LP_STBY_DEBOUNCE_MS)
    {
        if (!btn_pressed_raw())
        {
            reenter_standby_now();           // 눌리지 않았다 → 즉시 재진입
        }
        HAL_Delay(1);
        stable_ms++;
    }

    // (B) 홀드 구간(1초)
    uint16_t hold = 0;
    while (hold < LP_STBY_WAKE_HOLD_MS)
    {
        if (!btn_pressed_raw())
        {
            reenter_standby_now();           // 유지 실패 → 즉시 재진입
        }
        HAL_Delay(1);
        hold++;
    }

    // 여기 도달 = “1초 길게 눌러 켜기” 성공 → 정상 부팅 진행
}

void lp_stby_init(void)
{
    bool pressed = btn_pressed_raw();

    s_last_pressed   = pressed;		  // 0: pressed(활성저레벨 가정), 1: not pressed
    s_stable_pressed = pressed;
    s_stable_ms      = 0;
    s_press_ms       = 0;
    s_fired          = false;
}

static void wkup1_config_for_active_low(void)
{
    // 0) Disable 먼저
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 1) 폴라리티 = LOW 활성
//    SET_BIT(PWR->WUCR3, PWR_WUCR3_WUPP1);
//
//    // 2) 내부 Pull = NOPULL (외부 10 kΩ 사용)
//    MODIFY_REG(PWR->WUCR2, PWR_WUCR2_WUPPUPD1_Msk, (0U << PWR_WUCR2_WUPPUPD1_Pos));

    // 3) Wake flag/WUF1 클리어 (1을 써서 클리어)
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);

    // (선택) SBF도 정리
#ifdef PWR_WUSR_SBF
    WRITE_REG(PWR->WUSR, PWR_WUSR_SBF);
#endif

    // 4) Enable
    SET_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
}


void lp_stby_prepare_before(void)
{
    // 사용자가 오버라이드:
    // - 모터/버저/LED OFF
    // - 상태 저장(Flash/EEPROM)
    // - 주변장치 안전 종료
}

static void lp_stby_enter(void)
{
    lp_stby_prepare_before();

    __disable_irq();
    HAL_SuspendTick();

    // Wake-up 소스는 CubeMX에서 설정(예: RTC Alarm, WKUP 핀)
    wkup1_config_for_active_low();
    HAL_PWR_EnterSTANDBYMode();

//    while (1)
//    {
//        // 돌아오지 않음
//    }
}

void lp_stby_force(void)
{
    s_fired = true;
    lp_stby_enter();
}

static bool wkup1_arm_for_pa0(void)
{
    // 0) Disable
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 1) 소스 선택: WUCR3.WUSEL1을 PA0에 맞는 값으로 설정
//    MODIFY_REG(PWR->WUCR3, PWR_WUCR3_WUSEL1_Msk,
//               (WKUP1_SEL_FOR_PA0 << PWR_WUCR3_WUSEL1_Pos));

    // 2) 플래그 클리어
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);

    // 3) Enable
    SET_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 4) Enable 직후 플래그 체크 → 활성로 보이면 취소
    if (READ_BIT(PWR->WUSR, PWR_WUSR_WUF1)) {
        CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
        WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);
        return false;
    }
    return true;
}

static void enter_standby_safe(void)
{
    // 릴리즈(High) 최종 소프트 체크
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_SET)
    	return;

    lp_stby_prepare_before();

    if (!wkup1_arm_for_pa0())
    	return;     // 활성로 보이면 진입 취소

    __disable_irq();
    HAL_SuspendTick();
    HAL_PWR_EnterSTANDBYMode();
    while (1) { }
}

void lp_stby_on_1ms(void)
{
    bool raw = btn_pressed_raw();   // LOW=눌림

    // --- 디바운스 갱신 ---
    if (raw == s_last_pressed)
    {
        if (s_stable_ms < 0xFFFF)
        {
            s_stable_ms++;
        }
    }
    else
    {
        s_stable_ms   = 0;
        s_last_pressed = raw;
    }

    if (s_stable_ms == LP_STBY_DEBOUNCE_MS)
    {
        s_stable_pressed = raw;     // 여기서 비로소 true/false로 “안정된 상태” 확정
    }

    // --- 상태머신 ---
    static enum { IDLE, HOLDING, ARMED } s_state = IDLE;

    switch (s_state)
    {
        case IDLE:
        {
            if (s_stable_pressed)   // 눌림(디바운스 통과)
            {
                if (s_press_ms < 0x7FFFFFFF)
                {
                    s_press_ms++;
                }

                if(s_press_ms >= 300)
                {
                	buzzer_play_shutdown_pororororong();
                }

                if (s_press_ms >= LP_STBY_HOLD_MS)  // 1초 이상 눌림
                {
                    s_state = HOLDING;
                }

            }
            else
            {
                s_press_ms = 0;
            }
        } break;

        case HOLDING:
        {
            if (!s_stable_pressed)  // 손을 뗐을 때
            {
                s_state     = ARMED;
                s_stable_ms = 0;    // 아래 ARMED 단계의 릴리즈 안정 대기에 재사용
            }
        } break;

        case ARMED:
        {
            if (!s_stable_pressed)  // 손 뗀 상태가 계속 유지될 때
            {
                if (s_stable_ms >= 50)    // 50ms 안정 유지
                {
                    enter_standby_safe(); // WKUP arm + Standby (복귀 없음)
                }
            }
            else
            {
                // 다시 눌리면 취소
                s_state    = IDLE;
                s_press_ms = 0;
            }
        } break;
    }
}
