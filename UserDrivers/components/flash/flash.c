/*
 * flash.c
 *
 *  Created on: Sep 9, 2025
 *      Author: RCY
 */


#include "flash.h"


void flash_write_color_reference(uint8_t sensor_side, uint8_t color_index, reference_entry_t entry)
{
    uint32_t base_addr = (sensor_side == BH1749_ADDR_LEFT) ? FLASH_COLOR_TABLE_ADDR_LEFT : FLASH_COLOR_TABLE_ADDR_RIGHT;
    uint32_t addr = base_addr + color_index * FLASH_COLOR_ENTRY_SIZE;

    HAL_FLASH_Unlock();

    // entry의 RAM 주소를 8바이트씩 건네준다 (포인터)
    for (size_t i = 0; i < sizeof(entry) / 8; i++)
    {
        uint32_t src_ptr = (uint32_t)(uintptr_t)((uint8_t*)&entry + i * 8);  // ✅ 포인터 전달
        HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i * 8, src_ptr);
        if (st != HAL_OK) break;
    }

    HAL_FLASH_Lock();
}

reference_entry_t flash_read_color_reference(uint8_t sensor_side, uint8_t color_index)
{
    uint32_t base_addr = (sensor_side == BH1749_ADDR_LEFT) ? FLASH_COLOR_TABLE_ADDR_LEFT : FLASH_COLOR_TABLE_ADDR_RIGHT;
    uint32_t addr = base_addr + color_index * FLASH_COLOR_ENTRY_SIZE;

    reference_entry_t entry;
    memcpy(&entry, (void*)addr, FLASH_COLOR_ENTRY_SIZE);

    return entry;
}

static inline void u375_get_bank_page(uint32_t addr, uint32_t *bank, uint32_t *page)
{
    const uint32_t SPLIT = 0x08080000UL;      // U375 고정 분기
    if (addr < SPLIT) {
        *bank = FLASH_BANK_1;
        *page = (addr - 0x08000000UL) / FLASH_PAGE_SIZE;
    } else {
        *bank = FLASH_BANK_2;
        *page = (addr - SPLIT) / FLASH_PAGE_SIZE;
    }
}

void flash_erase_color_table(uint8_t sensor_side)
{
    uint32_t base = (sensor_side == BH1749_ADDR_LEFT)
                  ? FLASH_COLOR_TABLE_ADDR_LEFT
                  : FLASH_COLOR_TABLE_ADDR_RIGHT;

    uint32_t bank, page;
    u375_get_bank_page(base, &bank, &page);

    FLASH_EraseInitTypeDef ei = {0};
    uint32_t page_error;

    ei.TypeErase = FLASH_TYPEERASE_PAGES;
    ei.Banks     = bank;     // ★ 반드시 지정
    ei.Page      = page;     // ★ 해당 뱅크 기준 페이지
    ei.NbPages   = 1;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_FLASHEx_Erase(&ei, &page_error);
    HAL_FLASH_Lock();
}
