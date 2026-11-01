#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "boot/stm32f1xx_hal.h"
#include "../common/boot_counter.h"
static unsigned programCalls, failAt;
static int unlocked;
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { unlocked=1; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { unlocked=0; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *p,uint32_t *error) {
    assert(unlocked && p->TypeErase==FLASH_TYPEERASE_PAGES && p->NbPages==1);
    assert(p->PageAddress==0x0800F800U || p->PageAddress==0x0800FC00U);
    memset((void *)(uintptr_t)p->PageAddress,0xff,1024); *error=UINT32_MAX; return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t kind,uint32_t address,uint64_t value) {
    assert(unlocked && kind==FLASH_TYPEPROGRAM_WORD);
    assert(address>=0x0800F800U && address<=0x0800FFFCU && address%4==0);
    if (++programCalls==failAt) return HAL_ERROR;
    volatile uint32_t *p=(volatile uint32_t *)(uintptr_t)address;
    assert((*p & (uint32_t)value)==(uint32_t)value); *p=(uint32_t)value; return HAL_OK;
}
int main(void) {
#ifdef MAP_FIXED_NOREPLACE
    void *memory=mmap((void *)(uintptr_t)0x0800F000U,4096,PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
#else
    puts("boot journal: SKIP (MAP_FIXED_NOREPLACE unavailable)"); return 77;
#endif
    assert(memory!=MAP_FAILED); memset(memory,0xff,4096);
    for(uint32_t expected=1;expected<=400;++expected) assert(BootCounter_Next()==expected);
    assert(!unlocked);
    failAt=programCalls+2; assert(BootCounter_Next()==0); /* counter written, commit absent */
    failAt=0; assert(BootCounter_Next()==401);
    assert(BootCounter_Next()==402);
    /* Independent torn record must not masquerade as a committed boot. */
    for(uint32_t expected=403;expected<=800;++expected) assert(BootCounter_Next()==expected);
    assert(!unlocked); assert(munmap(memory,4096)==0);
    puts("boot journal: 800 boots/page rotation, torn commit recovery, flash relock PASS");
    return 0;
}
