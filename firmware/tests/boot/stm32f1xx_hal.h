#ifndef TEST_FLASH_HAL_H
#define TEST_FLASH_HAL_H
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
typedef struct { uint32_t TypeErase,PageAddress,NbPages; } FLASH_EraseInitTypeDef;
#define FLASH_FLAG_EOP 1U
#define FLASH_FLAG_PGERR 2U
#define FLASH_FLAG_WRPERR 4U
#define FLASH_TYPEERASE_PAGES 1U
#define FLASH_TYPEPROGRAM_WORD 2U
#define __HAL_FLASH_CLEAR_FLAG(flags) ((void)(flags))
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *,uint32_t *);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t,uint32_t,uint64_t);
#endif
