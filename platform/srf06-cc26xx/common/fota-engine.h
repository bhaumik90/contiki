#ifndef CC26XX_FOTA_ENGINE_H_
#define CC26XX_FOTA_ENGINE_H_

#define GOLDEN_IMAGE        0x19           //  Address where the factory-given firmware is stored in external flash (for backup)
#define FLASH_PAGE_SIZE     0x1000
#define METADATA_LENGTH     8
#define CURRENT_FIRMWARE    0x2
#define OTA_RESET_VECTOR    0x4     //  RESET ISR Vector (see )
#define OTA_METADATA_SPACE 0x100

#define REV16(val) 						(((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8))
#define REV32(val) 						(((val>>24) & 0xFF) | ((val<<8) & 0xFF0000) | \
															((val>>8) & 0xFF00) | ((val<<24) & 0xFF000000))

#endif /* CC26XX_FOTA_ENGINE_H_ */