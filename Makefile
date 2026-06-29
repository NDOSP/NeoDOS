.PHONY: all boot bios_error image tools clean run kernel modules

BUILD_DIR := build
MOD_DIR   := $(BUILD_DIR)/MODULES
DISK_IMG  := $(BUILD_DIR)/disk.img
BIOS_ERROR := $(BUILD_DIR)/bioserr.bin
MOD_BINS  := $(wildcard $(MOD_DIR)/*.mod)

all: boot bios_error image

kernel: | build
	$(MAKE) -C kernel
	cp kernel/build/kernel.elf build/NEOKRN.ELF

boot: | build
	$(MAKE) -C boot

tools: | build
	$(MAKE) -C tools
	cp -r tools/build/* build/

modules: | build
	$(MAKE) -C modules all
	@mkdir -p $(MOD_DIR)

build:
	@mkdir -p build

image: kernel boot tools modules bios_error
	@mkdir -p $(BUILD_DIR)
	
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64 status=progress
	
	printf "g\nn\n\n2048\n+60M\nt\n1\nw\n" | fdisk $(DISK_IMG)

	dd if=$(BIOS_ERROR) of=$(DISK_IMG) bs=440 count=1 conv=notrunc
	/bin/echo -ne '\x80' | dd of=$(DISK_IMG) bs=1 seek=446 conv=notrunc
	/bin/echo -ne '\x55\xAA' | dd of=$(DISK_IMG) bs=1 seek=510 conv=notrunc

	cp data/osdata.json data.json
	./build/ndrcreator encode
	rm -rf data.json
	mv output.ndr build/OSDATA.NDR

	cp data/font.json data.json
	./build/nffcreator data.json build/FONT.NFF
	rm -rf data.json
	
	FAT_IMG=build/fatpart.img; \
	dd if=/dev/zero of=$$FAT_IMG bs=1M count=60 status=progress 2>&1; \
	mkfs.vfat -F32 -n EFI $$FAT_IMG >/dev/null 2>&1; \
	MOD_PAIRS=""; \
	for m in $(MOD_BINS); do \
		name=$$(basename $$m); \
		MOD_PAIRS="$$MOD_PAIRS $$m=NEODOS/MODULES/$$name"; \
	done; \
	INIT_PAIR=""; \
	if [ -f build/INIT.ELF ]; then INIT_PAIR="build/INIT.ELF=NEODOS/INIT.ELF"; fi; \
	python3 populate_fat.py $$FAT_IMG \
		build/BOOTX64.EFI=EFI/BOOT/BOOTX64.EFI \
		build/OSDATA.NDR=NEODOS/OSDATA.NDR \
		build/NEOKRN.ELF=NEODOS/NEOKRN.ELF \
		build/FONT.NFF=NEODOS/FONT.NFF \
		$$MOD_PAIRS $$INIT_PAIR && \
	dd if=$$FAT_IMG of=$(DISK_IMG) bs=512 seek=2048 conv=notrunc status=progress 2>&1; \
	rm -f $$FAT_IMG

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	rm -f $(DISK_IMG) $(BIOS_ERROR)
	rm -rf $(BUILD_DIR)

run: image
	./run.sh