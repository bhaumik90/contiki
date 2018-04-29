/**
 * \file:    FOTA Client Source File
 * \author:  Bhaumik Bhandari
 */
#include "fota-client-conf.h"
#include "contiki.h"
#include "contiki-net.h"
#include "fota-client.h"

#include "er-coap-engine.h"
#include "rest-engine.h"
/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
PROCESS(fota_client, "FOTA CLIENT PROCESS");
/*---------------------------------------------------------------------------*/
extern resource_t res_fota, res_fota_state, res_fota_cmd;
process_event_t fota_init, fota_verify;
uip_ipaddr_t fota_server_addr;
struct ctimer reset_ct;
/*---------------------------------------------------------------------------*/
static void reset()
{
	FOTA_ENGINE.reset_device();
}
/*---------------------------------------------------------------------------*/
void
client_chunk_handler(void *response)
{
  PRINTF("|%.*s\n", ((coap_packet_t *)response)->payload_len, (char *)((coap_packet_t *)response)->payload);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(fota_client, ev, data)
{
  PROCESS_BEGIN();
  PRINTF("FOTA CLIENT PROCESS STARTED...\n");

	static coap_packet_t request[1];
	FOTA_SERVER(&fota_server_addr);
	coap_init_engine();
  rest_init_engine();

  fota_init = process_alloc_event();
	fota_verify = process_alloc_event();
  rest_activate_resource(&res_fota, "5/1");
	rest_activate_resource(&res_fota_cmd, "5/0");

	while(1)
	{
		PROCESS_WAIT_EVENT();

		if(ev == fota_init) 
		{
			if(!FOTA_ENGINE.init())
			{
				coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0);
				coap_set_header_uri_path(request, "/5/0");
				coap_set_payload(request, linkaddr_node_addr.u8, UIP_LLADDR_LEN);
				COAP_BLOCKING_REQUEST(&fota_server_addr, UIP_HTONS(COAP_DEFAULT_PORT), request, client_chunk_handler);
			}
		}
		else if(ev == fota_verify)
		{
			if(!FOTA_ENGINE.verify_image())
			{
				PRINTF("Image Downloaded successfully.Installing..\n");
				ctimer_set(&reset_ct, CLOCK_SECOND, reset, NULL);
			}
		}
	}
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void 
fota_client_init()
{
	process_start(&fota_client, NULL);
}
/*---------------------------------------------------------------------------*/