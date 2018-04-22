#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FLASH_WORD_SIZE 4

/* Firmware Image Information */
struct fwMetaData {
  uint16_t version;
  uint32_t size;  /* Bytes */
  uint16_t crc16; /* TODO: to be replaced by AES128 for verifying */
} __attribute__((packed));
typedef struct fwMetaData fwMetaData_t;
/*---------------------------------------------------------------------------*/
unsigned short
crc16_add(unsigned char b, unsigned short acc)
{
  acc ^= b;
  acc  = (acc >> 8) | (acc << 8);
  acc ^= (acc & 0xff00) << 4;
  acc ^= (acc >> 8) >> 4;
  acc ^= (acc & 0xff00) >> 5;
  return acc;
}
/*---------------------------------------------------------------------------*/
unsigned short
crc16_data(const unsigned char *data, int len, unsigned short acc)
{
  int i;
  
  for(i = 0; i < len; ++i) {
    acc = crc16_add(*data, acc);
    ++data;
  }
  return acc;
}
/*---------------------------------------------------------------------------*/
int
main(int argc, char *argv[]) 
{
  uint32_t firmware_size = 0;
  //  (1) Open the firmware .bin file
  FILE *firmware_bin = fopen(argv[1], "rb");

  //  (2) Run the CRC16 calculation over the file.  Print result.
  uint16_t crc_result = 0x0000;

  uint8_t _word[FLASH_WORD_SIZE]; //  a 4-byte buffer
  size_t nret;

  while(1 == (nret = fread(_word, FLASH_WORD_SIZE, 1, firmware_bin))) 
  {
    crc_result = crc16_data(_word, FLASH_WORD_SIZE, crc_result);
  }
  printf("CRC: %#x\n", crc_result);

  fseek(firmware_bin, 0L, SEEK_END);
  firmware_size = ftell(firmware_bin);
  printf("Size: %d\n", firmware_size);

  //  (3) Close the .bin file.
  fclose(firmware_bin);

  //  (4) Generate OTA image metadata
  fwMetaData_t metadata;
  metadata.crc16 = crc_result;
  metadata.size = firmware_size;
  sscanf(argv[2], "%hxu", &(metadata.version));

  uint8_t output_buffer[sizeof(fwMetaData_t)];
  memcpy(output_buffer, (uint8_t *)&metadata, sizeof(fwMetaData_t));

  //  (5) Open the output firmware .bin file
  FILE *metadata_bin = fopen( "firmware-metadata.bin", "wb" );

  //  (6) Write the metadata
  fwrite(output_buffer, sizeof(output_buffer), 1, metadata_bin);

  //  (7) 0xff spacing until firmware binary starts
  uint8_t blank_buffer[248];
  for (int b=0; b<248; b++) {
    blank_buffer[ b ] = 0xff;
  }
  fwrite(blank_buffer, 248, 1, metadata_bin);

  //  (8) Close the metadata file
  fclose(metadata_bin);

  return 0;
}
/*---------------------------------------------------------------------------*/