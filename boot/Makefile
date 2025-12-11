.PHONY: all bios uefi clean

BUILD_DIR := ../build
BOOT_SRC  := src

ASM := nasm
CC := gcc
LD := ld 

BIOS_SRCS := $(wildcard $(BOOT_SRC)/bios/*.asm)
BIOS_BINS := $(patsubst $(BOOT_SRC)/bios/%.asm,$(BUILD_DIR)/%.bin,$(BIOS_SRCS))

UEFI_SRCS := $(wildcard $(BOOT_SRC)/uefi/*.c)
UEFI_OBJS := $(patsubst $(BOOT_SRC)/uefi/%.c,$(BUILD_DIR)/%.o,$(UEFI_SRCS))
UEFI_CRT0 := /usr/lib/crt0-efi-x86_64.o
UEFI_EFI := $(BUILD_DIR)/BOOTX64.EFI

EFI_CFLAGS := -I/usr/include/efi -I/usr/include/efi/x86_64 \
              -I/usr/include/efi/protocol \
              -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
              -Wall -Wextra -ffreestanding -DEFI_FUNCTION_WRAPPER

all: bios uefi

bios: $(BIOS_BINS)

$(BUILD_DIR)/%.bin: $(BOOT_SRC)/bios/%.asm
	@mkdir -p $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

uefi: $(UEFI_EFI)

$(BUILD_DIR)/%.o: $(BOOT_SRC)/uefi/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(EFI_CFLAGS) -c $< -o $@

$(UEFI_EFI): $(UEFI_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(LD) -nostdlib -znocombreloc -shared -Bsymbolic \
          -T /usr/lib/elf_x86_64_efi.lds \
          $(UEFI_CRT0) $(UEFI_OBJS) \
          -L/usr/lib -lgnuefi -lefi \
          -o $(BUILD_DIR)/uefi.elf
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-x86_64 $(BUILD_DIR)/uefi.elf $@
	@rm -f $(BUILD_DIR)/uefi.elf
	@echo "Created: $@"
	@file $@ || true

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.EFI $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.elf