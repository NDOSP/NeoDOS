SRC_DIR := src
BUILD_DIR := build

CC := gcc
AS := nasm
LD := ld

CFLAGS := -ffreestanding -O2 -Wall -Wextra -nostdlib -nostartfiles -Isrc/ -MMD -MP
ASFLAGS := -f elf64
LDFLAGS := -T linker.ld -nostdlib

C_SOURCES := $(shell find $(SRC_DIR) -type f -name "*.c")
ASM_SOURCES := $(shell find $(SRC_DIR) -type f -name "*.asm")

C_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)
DEPS := $(C_OBJECTS:.o=.d)

TRAMPOLINE_BIN := $(BUILD_DIR)/trampoline.bin

all: build

# Trampoline binary (flat binary for AP startup)
$(TRAMPOLINE_BIN): boot/trampoline.asm
	mkdir -p $(BUILD_DIR)
	nasm -f bin $< -o $@

# C -> O
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ASM -> O
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

-include $(DEPS)

# smp_asm.asm incbin requires trampoline.bin to exist first
$(BUILD_DIR)/smp_asm.o: $(TRAMPOLINE_BIN)

build: $(TRAMPOLINE_BIN) $(OBJECTS)
	mkdir -p $(BUILD_DIR)
	cp linker.ld $(BUILD_DIR)/
	$(LD) $(OBJECTS) $(LDFLAGS) -o $(BUILD_DIR)/kernel.elf

clean:
	rm -rf $(BUILD_DIR)
