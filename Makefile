.PHONY: all boot bios_error image tools clean run

BUILD_DIR := build
DISK_IMG  := $(BUILD_DIR)/disk.img
BIOS_ERROR := $(BUILD_DIR)/bioserr.bin

all: boot bios_error image

boot:
	$(MAKE) -C boot

tools:
	$(MAKE) -C tools
	cp -r tools/build/* build/

image: boot tools bios_error
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
	
	@sudo bash -c '\
		LOOP=$$(losetup -f --show -o 1048576 $(DISK_IMG) 2>/dev/null); \
		if [ -z "$$LOOP" ]; then \
			LOOP=$$(losetup -f --show --partscan $(DISK_IMG)); \
			LOOP=$${LOOP}p1; \
		fi; \
		mkfs.vfat -F32 -n EFI $$LOOP >/dev/null 2>&1; \
		mkdir -p $(BUILD_DIR)/mnt; \
		mount $$LOOP $(BUILD_DIR)/mnt 2>/dev/null; \
		mkdir -p $(BUILD_DIR)/mnt/EFI/BOOT; \
		mkdir -p $(BUILD_DIR)/mnt/NEODOS; \
		cp $(BUILD_DIR)/BOOTX64.EFI $(BUILD_DIR)/mnt/EFI/BOOT/BOOTX64.EFI; \
		echo "Hi!" > $(BUILD_DIR)/mnt/NEODOS/TEST.TXT; \
		cp $(BUILD_DIR)/OSDATA.NDR $(BUILD_DIR)/mnt/NEODOS/OSDATA.NDR; \
		sync; \
		umount $(BUILD_DIR)/mnt 2>/dev/null; \
		losetup -d $$LOOP 2>/dev/null || true; \
		rmdir $(BUILD_DIR)/mnt 2>/dev/null || true; \
	'

clean:
	$(MAKE) -C boot clean
	rm -f $(DISK_IMG) $(BIOS_ERROR)
	rm -rf $(BUILD_DIR)

run: image
	./run.sh