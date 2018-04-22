#include "fota-engine.h"
#include "ext-flash.h"
#include "ti-lib.h"
#include "core/lib/crc16.h"
#include <stdint.h>
#include "fota-client.h"
/*---------------------------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif
/*---------------------------------------------------------------------------------------------*/
/*
 * @fn      erase_extflash_page
 *
 * @brief   Erase a page of external flash, starting at the given address.
 *
 * @param   ext_address    - The external flash address representing the start
 *                           of the page to be erased.
 *
 * @return  0 or error code
 */
int
erase_extflash_page(uint32_t ext_address)
{
  //  (1) Open the external flash
  int eeprom_access = ext_flash_open();
  if(!eeprom_access) {
    PRINTF("[external-flash]:\tError - Could not access EEPROM.\n");
    ext_flash_close();
    return -1;
  }

  // Kick the watchdog. Use watchdog library instead? (beware of bootloader size)
  ti_lib_watchdog_reload_set(0xFFFFF);
  ti_lib_watchdog_int_clear();

  eeprom_access = ext_flash_erase(ext_address, FLASH_PAGE_SIZE);
  if(!eeprom_access) {
    PRINTF("[external-flash]:\tError - Could not erase EEPROM.\n");
    ext_flash_close();
    return -1;
  }

  ext_flash_close();

  return 0;
}
/*---------------------------------------------------------------------------------------------*/
static uint8_t
init(){

	PRINTF("Erasing OTA slot [%#x, %#x)...\n", (GOLDEN_IMAGE<<12), ((GOLDEN_IMAGE+25)<<12));
  for (int page=0; page<25; page++)
	{
    while(erase_extflash_page(((GOLDEN_IMAGE + page) << 12)));
  }
	return 0;
}
/*---------------------------------------------------------------------------------------------*/
static uint8_t
write_image(uint32_t bytesWritten, uint8_t *data, uint16_t data_len) 
{
  int eeprom_access;
  eeprom_access = ext_flash_open();

  if(!eeprom_access) {
    PRINTF("[external-flash]:\tError - could not access EEPROM.\n");
    ext_flash_close();
    return 1;
  }

  eeprom_access = ext_flash_write((GOLDEN_IMAGE<<12)+bytesWritten, data_len, data);

  if(!eeprom_access) {
    PRINTF("[external-flash]:\tError - Could not write to EEPROM.\n");
    ext_flash_close();
    return 1;
  }

  ext_flash_close();

  PRINTF("[external-flash]:\tFirmware %d bytes successfully written to %#lx.\n", data_len, (GOLDEN_IMAGE<<12)+bytesWritten);
  return 0;
}
/*---------------------------------------------------------------------------------------------*/
static void
reset_chip() 
{ 
  PRINTF("RESETING DEVICE\n");
	ti_lib_sys_ctrl_system_reset();
}
/*---------------------------------------------------------------------------------------------*/
static uint8_t
verify_img()
{
	int eeprom_access = ext_flash_open();
  if(!eeprom_access) {
    PRINTF("[external-flash]:\tError - Could not access EEPROM.\n");
    ext_flash_close();
    return -1;
  }

	fwMetaData_t otaImgMetaData;

	eeprom_access = ext_flash_read((GOLDEN_IMAGE<<12), sizeof(fwMetaData_t), (uint8_t *)&otaImgMetaData);
	if(!eeprom_access) {
		PRINTF("[external-flash]:\tError - Could not read EEPROM.\n");
		ext_flash_close();
		return -1;
	}

  uint32_t firmware_address = (GOLDEN_IMAGE<<12) + OTA_METADATA_SPACE;
  uint32_t firmware_end_address = firmware_address + otaImgMetaData.size;
  uint16_t imageCRC = 0; uint8_t _word[4];

	PRINTF("Recomputing CRC16 on external flash image within range %#lx to %#lx\n", firmware_address, firmware_end_address);

  while (firmware_address < firmware_end_address) 
	{
		eeprom_access = ext_flash_read(firmware_address, 4, _word);
    if(!eeprom_access) {
      PRINTF("[external-flash]:\tError - Could not read EEPROM.\n");
      ext_flash_close();
      return -1;
    }
		imageCRC = crc16_data(_word, 4, imageCRC);
    firmware_address += 4; // move 4 bytes forward
  }

  PRINTF("CRC Calculated: %#x\n", imageCRC);
	if(imageCRC==otaImgMetaData.crc16)
	{
		PRINTF("Image Verified\n");
		PRINTF("OTA Version %04x: \n", otaImgMetaData.version);
		PRINTF("OTA Size %08lx: \n", otaImgMetaData.size);
		PRINTF("OTA Version %04x: \n", otaImgMetaData.crc16);
		return 0;
	}
	else
	{
		PRINTF("Image NOT Verified\n");
		return 1;
	}
}
/*---------------------------------------------------------------------------------------------*/
const fota_engine_t srf06_cc26xx_fota = {

	"CC26XX CC13XX FOTA ENGINE",
	init,
	write_image,
	reset_chip,
	verify_img
};
/*---------------------------------------------------------------------------------------------*/