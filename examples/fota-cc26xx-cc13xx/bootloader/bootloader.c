#include <stdint.h>
#include "ti-lib.h"
#include "ext-flash.h"
#include "fota-engine.h"
#include "fota-client.h"
#include "crc16.h"
#include "flash.h"

static void
power_domains_on(void) {
  /* Turn on the PERIPH PD */
  ti_lib_prcm_power_domain_on(PRCM_DOMAIN_PERIPH);

  /* Wait for domains to power on */
  while((ti_lib_prcm_power_domain_status(PRCM_DOMAIN_PERIPH)
        != PRCM_DOMAIN_POWER_ON));
}

static void
getOtaSlotMetadata(fwMetaData_t *otaSlotMetaData)
{
  int eeprom_access = ext_flash_open();
  if(!eeprom_access) { ext_flash_close(); }

  eeprom_access = ext_flash_read(GOLDEN_IMAGE<<12, sizeof(fwMetaData_t), (uint8_t*)otaSlotMetaData);
  if(!eeprom_access) { ext_flash_close(); }

  ext_flash_close();
}

static void
bootImage()
{
  uint32_t destination_address = (CURRENT_FIRMWARE<<12) + OTA_METADATA_SPACE + OTA_RESET_VECTOR;

  ti_lib_int_master_disable();
  HWREG(NVIC_VTABLE) = (CURRENT_FIRMWARE<<12) + OTA_METADATA_SPACE;
  ti_lib_int_master_enable();

  __asm("LDR R0, [%[dest]]"::[dest]"r"(destination_address));
  __asm("ORR R0, #1");
  __asm("BX R0");
}

void
initialize_peripherals() 
{
  /* Disable global interrupts */
  bool int_disabled = ti_lib_int_master_disable();

  power_domains_on();

  /* Enable GPIO peripheral */
  ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_GPIO);

  /* Apply settings and wait for them to take effect */
  ti_lib_prcm_load_set();
  while(!ti_lib_prcm_load_get());

  /* Make sure the external flash is in the lower power mode */
  ext_flash_init();

  /* Re-enable interrupt if initially enabled. */
  if(!int_disabled) {
    ti_lib_int_master_enable();
  }
}

static void
getCurrentMetadata(uint8_t *metadata)
{
  uint8_t *pui8ReadAddress = (uint8_t *)(CURRENT_FIRMWARE<<12), len = METADATA_LENGTH;
  while(len--) *metadata++ = *pui8ReadAddress++;
}

static uint8_t
verifyOtaImage(fwMetaData_t *otaSlotMetaData)
{
  int eeprom_access = ext_flash_open();
  if(!eeprom_access) { ext_flash_close(); return -1; }

  uint32_t firmware_address = (GOLDEN_IMAGE<<12) + OTA_METADATA_SPACE;
  uint32_t firmware_end_address = firmware_address + otaSlotMetaData->size;
  uint16_t imageCRC = 0; uint8_t _word[4];

  while(firmware_address < firmware_end_address) 
	{
		eeprom_access = ext_flash_read(firmware_address, 4, _word);
    if(!eeprom_access) { ext_flash_close(); return -1; }
		
    imageCRC = crc16_data(_word, 4, imageCRC);
    firmware_address += 4; // move 4 bytes forward
  }

	if(imageCRC==otaSlotMetaData->crc16) return 0;
	else return 1;
}

static void 
installImage()
{
  uint8_t fwData[4096];

  int eeprom_access = ext_flash_open();
  if(!eeprom_access) { ext_flash_close(); }

  // Erase the current image area
  for(uint8_t i=0; i<25; i++)
  {
    if(FlashSectorErase((CURRENT_FIRMWARE<<12)+4096*i)!=FAPI_STATUS_SUCCESS)
    {
      ext_flash_close(); return;
    }

    eeprom_access = ext_flash_read(((GOLDEN_IMAGE<<12)+4096*i), 4096, fwData);
    if(!eeprom_access) { ext_flash_close(); }

    while(FlashProgram(fwData, ((CURRENT_FIRMWARE<<12)+4096*i), 4096) != FAPI_STATUS_SUCCESS);
  }

  ext_flash_close();
}

static int
eraseOtaSlot()
{
  int eeprom_access = ext_flash_open();
  if(!eeprom_access) { ext_flash_close(); return -1; }

  for (int page=0; page<25; page++)
	{
    // Kick the watchdog. Use watchdog library instead? (beware of bootloader size)
    ti_lib_watchdog_reload_set(0xFFFFF);
    ti_lib_watchdog_int_clear();

    eeprom_access = ext_flash_erase(((GOLDEN_IMAGE + page) << 12), FLASH_PAGE_SIZE);
    if(!eeprom_access) { ext_flash_close(); return -1; }
  }
  ext_flash_close();
  return 0;
}

int
main(void)
{
  initialize_peripherals();

  fwMetaData_t currentMetadata, otaMetadata;

  getCurrentMetadata((uint8_t *)&currentMetadata);
  getOtaSlotMetadata(&otaMetadata);

  // Any currently installed images
  if(currentMetadata.version==0xFFFF)
  {
    // No img installed..let's check if any is available in ext mermory
    if(otaMetadata.version==0xFFFF)
    {
      // No img available in external flash
      // Human intervention needed
    }
    else
    {
      // We have an image in ext flash...let's verify it
      if(!verifyOtaImage(&otaMetadata))
      {
        // Verification successful. Let's install it
        installImage();
        ti_lib_sys_ctrl_system_reset();
      }
    }
  }
  else
  {
    // Image present in currently. Let's check if we have better image stored
    if((otaMetadata.version==0xFFFF) || (otaMetadata.version <= currentMetadata.version))
    {
      // Image either not present or an old version. Lets boot our current image
      eraseOtaSlot();
      bootImage();
    }
    else
    {
      // We have a better image in external memory. Let's verify it
      if(!verifyOtaImage(&otaMetadata))
      {
        // Verification successful. Let's install it
        installImage();
        eraseOtaSlot();
        ti_lib_sys_ctrl_system_reset();
      }
    }
  }

  //  main() *should* never return - we should have rebooted or branched
  //  to other code by now.
  return 0;
}
