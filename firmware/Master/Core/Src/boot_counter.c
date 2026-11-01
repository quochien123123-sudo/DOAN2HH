#include "boot_counter.h"
#include "stm32f1xx_hal.h"
#define JOURNAL_A 0x0800F800UL
#define JOURNAL_B 0x0800FC00UL
#define PAGE_BYTES 1024UL
#define RECORD_BYTES 8UL
static uint32_t word(uint32_t address) { return *(volatile const uint32_t *)(uintptr_t)address; }
uint32_t BootCounter_Next(void) {
    uint32_t maximum=0U, active=JOURNAL_A, address;
    for (uint32_t page=JOURNAL_A;page<=JOURNAL_B;page+=PAGE_BYTES) {
        for (address=page;address<page+PAGE_BYTES;address+=RECORD_BYTES) {
            uint32_t value=word(address), check=word(address+4U);
            if (value!=0U && value!=UINT32_MAX && check==~value && value>maximum) {
                maximum=value; active=page;
            }
        }
    }
    if (maximum>=UINT32_MAX-1U) return 0U;
    for (address=active;address<active+PAGE_BYTES;address+=RECORD_BYTES)
        if (word(address)==UINT32_MAX && word(address+4U)==UINT32_MAX) break;
    if (HAL_FLASH_Unlock()!=HAL_OK) return 0U;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPERR);
    HAL_StatusTypeDef status=HAL_OK;
    if (address==active+PAGE_BYTES) {
        address=(active==JOURNAL_A)?JOURNAL_B:JOURNAL_A;
        FLASH_EraseInitTypeDef erase={0}; uint32_t error=0U;
        erase.TypeErase=FLASH_TYPEERASE_PAGES; erase.PageAddress=address; erase.NbPages=1U;
        status=HAL_FLASHEx_Erase(&erase,&error);
    }
    uint32_t next=maximum+1U;
    if (status==HAL_OK) status=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address,next);
    if (status==HAL_OK) status=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address+4U,(uint32_t)~next);
    (void)HAL_FLASH_Lock();
    if (status!=HAL_OK || word(address)!=next || word(address+4U)!=(uint32_t)~next) return 0U;
    return next;
}
