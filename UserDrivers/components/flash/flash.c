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
    for (size_t i = 0; i < sizeof(entry)/8; i++)
    {
        uint32_t src_ptr = (uint32_t)(uintptr_t)((uint8_t*)&entry + i*8);  // ✅ 포인터 전달
        HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i*8, src_ptr);
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

void flash_erase_color_table(uint8_t sensor_side)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error;

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page      = (sensor_side == BH1749_ADDR_LEFT) ? (FLASH_COLOR_TABLE_ADDR_LEFT - 0x08000000) / FLASH_PAGE_SIZE : (FLASH_COLOR_TABLE_ADDR_RIGHT - 0x08000000) / FLASH_PAGE_SIZE;
    erase_init.NbPages   = 1;

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&erase_init, &page_error);
    HAL_FLASH_Lock();
}
