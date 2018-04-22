all: fota-client-test

CONTIKI=../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

OTA_IMAGE=1

SMALL=1

APPS += fota-client

CONTIKI_WITH_IPV6 = 1

include $(CONTIKI)/Makefile.include
