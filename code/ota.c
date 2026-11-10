#include "main.h"
#include "ota_layout.h"
#include "ota.h"

void OTA_Trigger(void)
{
    FLASH_EraseInitTypeDef e = {0};
    uint32_t err = 0;
    uint32_t upd[2];
    uint8_t  i;

    upd[0] = OTA_UPD_MAGIC;
    upd[1] = ~OTA_UPD_MAGIC;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    e.TypeErase    = FLASH_TYPEERASE_SECTORS;
    e.Sector       = OTA_META_SECTOR;
    e.NbSectors    = 1;
    e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&e, &err) == HAL_OK) {
        for (i = 0; i < 2U; i++)
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, OTA_META_ADDR + 4U * i, upd[i]);
    }
    HAL_FLASH_Lock();

    HAL_Delay(20);
    NVIC_SystemReset();
}
