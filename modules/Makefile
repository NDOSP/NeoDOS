MODULES_DIR := ../build/MODULES
CC := gcc
CFLAGS := -ffreestanding -O2 -Wall -Wextra -nostdlib -nostartfiles \
          -fno-pie -fno-stack-protector -mno-red-zone \
           -Iinclude -MMD -MP
AS := nasm
ASFLAGS := -f elf64
LD := ld
LDFLAGS := -nostdlib -static

C_SRCS := $(wildcard src/*.c)
ASM_SRCS := $(wildcard src/*.asm)

C_OBJS := $(patsubst src/%.c,$(MODULES_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst src/%.asm,$(MODULES_DIR)/%.o,$(ASM_SRCS))
ALL_OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(C_OBJS:.o=.d)
MODS := $(patsubst src/%.c,$(MODULES_DIR)/%.mod,$(C_SRCS)) \
        $(patsubst src/%.asm,$(MODULES_DIR)/%.mod,$(ASM_SRCS))

.PHONY: all clean init

all: init $(MODS)

init: $(MODULES_DIR)/init.o
	$(LD) $(LDFLAGS) -T init.ld $(MODULES_DIR)/init.o -o ../build/INIT.ELF

$(MODULES_DIR)/%.o: src/%.c
	@mkdir -p $(MODULES_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(MODULES_DIR)/%.o: src/%.asm
	@mkdir -p $(MODULES_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(MODULES_DIR)/%.mod: $(MODULES_DIR)/%.o
	$(LD) -r $< -o $@

-include $(DEPS)

clean:
	rm -f $(MODULES_DIR)/*.o $(MODULES_DIR)/*.d $(MODULES_DIR)/*.mod ../build/INIT.ELF
