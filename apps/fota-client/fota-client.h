/*
 *	/file:
 *      Fota Client header file
 *
 *	/author: Bhaumik Bhandari
 *
*/
#ifndef _FOTA_CLIENT_H_
#define _FOTA_CLIENT_H_
/*---------------------------------------------------------------------------*/
#include <stdint.h>
/*---------------------------------------------------------------------------*/
/* Firmware Image Metadata */
struct fwMetaData {
  uint16_t version;
  uint32_t size;  /* Bytes */
  uint16_t crc16;
} __attribute__((packed));
typedef struct fwMetaData fwMetaData_t;
/*---------------------------------------------------------------------------*/
/* 
 * Board Specific FOTA Implementation 
 * 
 * terms: Memory - means external or on-chip flash.
 *
 * desc: These APIs link application layer to fota manager on the device. 
 * This is mapped to baord / platform
*/
typedef struct fota_engine {

	char *name;

  uint8_t (*init)(); /* Clears the memory where image is to be written */

  uint8_t (*write_image)(uint32_t bytesWritten, uint8_t *data, uint16_t data_len); /* Writes data to the memory */

  void (*reset_device)(); /* Reset Device */

  uint8_t (*verify_image)();
} fota_engine_t;
/*---------------------------------------------------------------------------*/
extern const struct fota_engine FOTA_ENGINE;
void fota_client_init();
/*---------------------------------------------------------------------------*/
#endif /* _FOTA_CLIENT_H_ */