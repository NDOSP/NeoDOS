MODULES_DIR := ../build/MODULES
CC := gcc
CFLAGS := -ffreestanding -O2 -fPIC -Wall -Wextra -nostdlib -nostartfiles \
          -fno-stack-protector -mno-red-zone \
          -Iinclude -MMD -MP \
		  -I../libs
AS := nasm
ASFLAGS := -f elf64
LD := ld
LDFLAGS := -nostdlib -static
OBJCOPY := objcopy

MODULE_NAMES := $(patsubst %/src,%,$(wildcard */src))
MODS := $(addprefix $(MODULES_DIR)/, $(addsuffix .mod, $(MODULE_NAMES)))

.PHONY: all clean

all: $(MODS)

define MODULE_RULE
# Compile
$(MODULES_DIR)/$(1).o: $(1)/src/$(1).c
	@mkdir -p $$(MODULES_DIR)
	$$(CC) $$(CFLAGS) -I$(1)/include -c $$< -o $$@

# Link
$(MODULES_DIR)/$(1).mod: $(MODULES_DIR)/$(1).o
	$$(LD) $$(LDFLAGS) -T mod.ld $$< -o $(MODULES_DIR)/$(1).elf
	$$(OBJCOPY) -O binary --set-section-flags .bss=alloc,load,contents $(MODULES_DIR)/$(1).elf $$@
	rm -f $(MODULES_DIR)/$(1).elf
endef

$(foreach mod,$(MODULE_NAMES),$(eval $(call MODULE_RULE,$(mod))))

$(MODULES_DIR)/%.d: ;

clean:
	rm -f $(MODULES_DIR)/*.o $(MODULES_DIR)/*.d $(MODULES_DIR)/*.mod
