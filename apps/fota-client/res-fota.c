/**
 * \file:    FOTA resource
 * \author:  Bhaumik Bhandari
 */

#include "rest-engine.h"
#include "fota-client.h"
#include "er-coap.h"
#include "dev/leds.h"
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
uint16_t prev_block_no = 0xffff;
extern struct process fota_client;
extern process_event_t fota_verify;
/*---------------------------------------------------------------------------------------------*/
static void
res_put_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
/*---------------------------------------------------------------------------------------------*/
RESOURCE(res_fota,
         "title=\"Fota\";rt=\"block\"",
				 NULL,
         NULL,
         res_put_handler,
         NULL);
/*---------------------------------------------------------------------------------------------*/
static void
res_put_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	coap_packet_t *const coap_req = (coap_packet_t *)request;
	size_t len = coap_req->payload_len;
	unsigned int ct = -1;

	if(!REST.get_header_content_type(request, &ct)) 
	{
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		const char *error_msg = "NoContentType";
		REST.set_response_payload(response, error_msg, strlen(error_msg));
		return;
	}

	leds_toggle(LEDS_RED);

	if((coap_req->block1_num > prev_block_no) || (prev_block_no == 0xffff)) 
	{
		PRINTF("BLOCK NO.: %lu\r\n", coap_req->block1_num);
		if(!FOTA_ENGINE.write_image(coap_req->block1_num*REST_MAX_CHUNK_SIZE, coap_req->payload, len))
		{
			prev_block_no = coap_req->block1_num;
		}
	}
	else
	{
		PRINTF("BLOCK REPEATED...NOT FLASHING!!!\r\n");
	}

	if(coap_req->block1_more)
	{
		REST.set_response_status(response, CONTINUE_2_31);
		coap_set_header_block1(response, coap_req->block1_num, 0, coap_req->block1_size);
	}
	else
	{
		REST.set_response_status(response, REST.status.CHANGED);
		coap_set_header_block1(response, coap_req->block1_num, 0, coap_req->block1_size);
		process_post(&fota_client, fota_verify, NULL);
	}
}
/*---------------------------------------------------------------------------------------------*/