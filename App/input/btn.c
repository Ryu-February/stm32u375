/*
 * btn.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "btn.h"
#include "uart.h"



typedef struct
{
	GPIO_TypeDef *port;
	uint16_t	  pin;
}btn_ch_cfg_t;


static const btn_ch_cfg_t btn_cfg[BTN_COUNT] =
{
		[BTN_GO]		= { GPIOB, GPIO_PIN_0  },
		[BTN_DELETE]   	= { GPIOB, GPIO_PIN_1  },
		[BTN_RESUME]   	= { GPIOB, GPIO_PIN_2  },
		[BTN_FORWARD]  	= { GPIOB, GPIO_PIN_12 },
		[BTN_BACKWARD] 	= { GPIOB, GPIO_PIN_13 },
		[BTN_LEFT]     	= { GPIOB, GPIO_PIN_14 },
		[BTN_RIGHT]    	= { GPIOB, GPIO_PIN_15 },
};


typedef struct
{
	volatile uint8_t stable;		// 1 = pressed
	volatile uint8_t cntr;
	volatile uint8_t press_flag;
}btn_state_t;

static btn_state_t 	s_btn[BTN_COUNT];
static uint32_t 	s_uptime_ms = 0;	//btn_update_1ms()가 올려주는 부팅 후 경과시간(ms)
// 활성화 마스크: 1=활성, 0=비활성
static uint32_t 	s_enable_mask = 0xFFFFFFFFu; // 기본 전체 활성


static inline uint8_t btn_read_raw(btn_id_t id)
{
	GPIO_PinState st = HAL_GPIO_ReadPin(btn_cfg[id].port, btn_cfg[id].pin);

	return (st == GPIO_PIN_RESET) ? 1u : 0u;// Low면 눌림
}

static inline bool btn_enabled(btn_id_t id)
{
    return ((s_enable_mask >> id) & 1u) != 0u;
}

void btn_init(void)
{
    // GPIO 클록/모드/풀업은 CubeMX에서 이미 설정되어 있다고 가정
    for (int i = 0; i < BTN_COUNT; i++)
    {
        uint8_t raw	        = btn_read_raw((btn_id_t)i);
        s_btn[i].stable     = raw;
        s_btn[i].cntr       = 0u;
        s_btn[i].press_flag = 0u;
    }
    s_uptime_ms = 0;
}

void btn_update_1ms(void)
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        // ★ 비활성 버튼은 즉시 무시 (카운터/플래그 정리)
        if (!btn_enabled((btn_id_t)i))
        {
            s_btn[i].cntr       = 0u;
            s_btn[i].press_flag = 0u;
            continue;
        }

        uint8_t raw = btn_read_raw((btn_id_t)i);

        if (raw == s_btn[i].stable)
        {
            s_btn[i].cntr = 0u;
            continue;
        }

        if (++s_btn[i].cntr >= BTN_DEBOUNCE_MS)
        {
            uint8_t prev   = s_btn[i].stable;
            s_btn[i].stable = raw;
            s_btn[i].cntr   = 0u;

            if (prev == 0u && raw == 1u)  // 눌림 엣지
            {
                s_btn[i].press_flag = 1u; // 여기선 enabled가 보장됨
            }
        }
    }
}

bool btn_is_pressed(btn_id_t id)
{
	// 비활성일 땐 항상 not-pressed로 간주
	if (!btn_enabled(id))
		return false;

	return (s_btn[id].stable != 0u);
}

bool btn_get_press(btn_id_t id)
{
	if (!btn_enabled(id))
		return false;

	if (s_btn[id].press_flag)
	{
		s_btn[id].press_flag = 0u;
		return true;
	}
		return false;
}

bool btn_pop_any_press(btn_id_t *out_id)
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
    	if (!btn_enabled((btn_id_t)i))
    		continue;

        if (s_btn[i].press_flag)
        {
            s_btn[i].press_flag = 0u;
            if (out_id) *out_id = (btn_id_t)i;
            	return true;
        }
    }
    return false;
}

void btn_enable_mask_set(uint32_t mask)
{
    s_enable_mask = mask;
    // 비활성 버튼들의 대기 중 이벤트는 폐기
    for (int i = 0; i < BTN_COUNT; i++)
    {
        if (((s_enable_mask >> i) & 1u) == 0u)
            s_btn[i].press_flag = 0u;
    }
}

uint32_t btn_enable_mask_get(void)
{
    return s_enable_mask;
}

static const char *btn_name(btn_id_t id)
{
    switch (id)
    {
        case BTN_GO:        return "GO";
        case BTN_DELETE:    return "DELETE";
        case BTN_RESUME:    return "RESUME";
        case BTN_FORWARD:   return "FORWARD";
        case BTN_BACKWARD:  return "BACKWARD";
        case BTN_LEFT:      return "LEFT";
        case BTN_RIGHT:     return "RIGHT";
        default:            return "?";
    }
}

void btn_print_states(void)
{
    // 각 버튼의 안정화된 현재 상태(1=pressed, 0=released)
    uart_printf("[BTN] GO:%d DEL:%d RES:%d F:%d B:%d L:%d R:%d\r\n",
                btn_is_pressed(BTN_GO),
                btn_is_pressed(BTN_DELETE),
                btn_is_pressed(BTN_RESUME),
                btn_is_pressed(BTN_FORWARD),
                btn_is_pressed(BTN_BACKWARD),
                btn_is_pressed(BTN_LEFT),
                btn_is_pressed(BTN_RIGHT));
}



void btn_print_events(void)
{
    // 이 호출 타이밍에 발생한 '눌림 엣지' 이벤트만 뽑아서 출력
    // (여기서 pop 하므로, 이후에 같은 이벤트는 사라짐)
	const char *sep = "";// 이번 틱에 발생한 "눌림 엣지"들을 모두 모아서 한 줄로 출력
    bool any = false;

    for (int i = 0; i < BTN_COUNT; i++)
    {
        btn_id_t id = (btn_id_t)i;
        if (btn_get_press(id))
        {
        	if(!any)
        	{
        		uart_printf("[BTN] press: ");
				any = true;
        	}
        	uart_printf("%s%s", sep, btn_name(id));
			sep = ", ";
        }
    }

    if (any)
    {
        uart_printf("\r\n");
    }
}

void btn_print_one(btn_id_t id)
{
	const char *name = btn_name(id);
	if (name == NULL) name = "UNKNOWN";  // 이중 안전장치

	uart_printf("[BTN] press: %s\r\n", name);
}
