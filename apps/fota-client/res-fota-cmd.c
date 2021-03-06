/**
 * \file:    FOTA resource
 * \author:  Bhaumik Bhandari
 */

#include "rest-engine.h"
#include "fota-client.h"
#include "er-coap.h"
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

extern uint16_t prev_block_no;
extern struct process fota_client;
extern process_event_t fota_init;
/*---------------------------------------------------------------------------------------------*/
static void
res_put_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
/*---------------------------------------------------------------------------------------------*/
RESOURCE(res_fota_cmd,
         "title=\"Fota Command\" POST/PUT cmd;rt=\"FOTA STATE\"",
         NULL,
         NULL,
         res_put_handler,
         NULL);
/*---------------------------------------------------------------------------------------------*/
static void
res_put_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) 
{	
	unsigned int ct = -1;

	if(!REST.get_header_content_type(request, &ct)) {
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		const char *error_msg = "NoContentType";
		REST.set_response_payload(response, error_msg, strlen(error_msg));
		return;
	}

	REST.set_response_status(response, REST.status.CHANGED);
	process_post(&fota_client, fota_init, NULL);
	prev_block_no = 0xFFFF; // TODO: remove this and make it static
	// TODO: Check if we an image with same metadata already stored
	// TODO: Add fota priority flag
}
/*---------------------------------------------------------------------------------------------*/