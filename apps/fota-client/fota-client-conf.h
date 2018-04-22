/**
 * \file
 *      FOTA Client Configuration file
 * \author
 *      Bhaumik Bhandari <bhaumik.bhandari@gmail.com>
 */
/*---------------------------------------------------------------------------*/
#ifndef __FOTA_CLIENT_CONF_H__
#define __FOTA_CLIENT_CONF_H__
/*---------------------------------------------------------------------------*/
#ifndef FOTA_SERVER
#define FOTA_SERVER(ipaddr)   uip_ip6addr(ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0x1)
#endif
/*---------------------------------------------------------------------------*/
/* IP buffer size must match all other hops, in particular the border router. */
#ifndef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE           256
#endif
/*---------------------------------------------------------------------------*/
/* Increase rpl-border-router IP-buffer when using more than 64. */
#ifndef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE            	64
#endif
/*---------------------------------------------------------------------------*/
/* Multiplies with chunk size, be aware of memory constraints. */
#undef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS     	4
/*---------------------------------------------------------------------------*/
/* Must be <= open transactions, default is COAP_MAX_OPEN_TRANSACTIONS-1. */
/*
   #undef COAP_MAX_OBSERVERS
   #define COAP_MAX_OBSERVERS           2
 */
/*---------------------------------------------------------------------------*/
/* Filtering .well-known/core per query can be disabled to save space. */
#undef  COAP_LINK_FORMAT_FILTERING
#define COAP_LINK_FORMAT_FILTERING     	0
#undef  COAP_PROXY_OPTION_PROCESSING
#define COAP_PROXY_OPTION_PROCESSING   	0
/*---------------------------------------------------------------------------*/
/* Enable client-side support for COAP observe */
#define COAP_OBSERVE_CLIENT 			1
/*---------------------------------------------------------------------------*/
#endif /* __FOTA_CLIENT_CONF_H__ */
