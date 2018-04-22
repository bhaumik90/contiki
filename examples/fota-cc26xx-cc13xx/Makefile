all: master bootloader ota-image

TARGET=srf06-cc26xx
BOARD=sensortag/cc1350

CONTIKI_EXAMPLES = ..

# ifndef VERSION
# 	${error "Mention the version of firmware image."}
# endif

%.o: %.c
	@gcc -std=c99 -c $< -o $@

generate-metadata: generate-metadata.o
	@gcc -o $@.out generate-metadata.o
	@rm -f *.o

.PHONY: bootloader ota-image master

master: ota-image bootloader
	@echo "Merging OTA Image with Bootloader"
	@srec_cat bootloader/bootloader.bin -binary -crop 0x0 0x2000 0x1FFA8 0x20000 ota_$(EXAMPLE)_$(VERSION).bin -binary -offset 0x2000 -crop 0x2000 0x1B000 -o master_$(EXAMPLE)_$(VERSION).bin -binary
	@rm -f ota_*.bin
	@cd bootloader; make clean; 

bootloader:
	@echo "Making OTA Bootloader";
	@cd ./bootloader; 				\
		make clean;						\
		make TARGET=$(TARGET) BOARD=$(BOARD);	\

ota-image: generate-metadata
ifndef EXAMPLE
	${error "Please define an EXAMPLE to add with bootloader."}
endif
ifndef VERSION
	${error "Please provide 16-bit firmware version. e.g. \"1.0\" as 0x0100 "}
endif
	@rm -f firmware*.bin;
	@echo "Making $(EXAMPLE)"
	@cd $(CONTIKI_EXAMPLES)/$(EXAMPLE); make clean TARGET=$(TARGET) BOARD=$(BOARD); make TARGET=$(TARGET) BOARD=$(BOARD) OTA_IMAGE=1;
	@echo "Adding metadata to $(EXAMPLE)";
	@./generate-metadata.out $(CONTIKI_EXAMPLES)/$(EXAMPLE)/$(EXAMPLE).bin $(VERSION)
	@srec_cat firmware-metadata.bin -binary $(CONTIKI_EXAMPLES)/$(EXAMPLE)/$(EXAMPLE).bin -binary -offset 0x100 -o ota_$(EXAMPLE)_$(VERSION).bin -binary
	@rm -f firmware-metadata.bin *.out
	@cd $(CONTIKI_EXAMPLES)/$(EXAMPLE); make clean;

clean:	
	@rm -f *.out *.bin;
	@cd ./bootloader; make clean;
ifdef EXAMPLE
	@cd $(CONTIKI_EXAMPLES)/$(EXAMPLE); make clean;
endif
	
