all: clean build

ASM_DIR := asmfiles
BUILD_DIR := build

ASM_SOURCES := $(wildcard $(ASM_DIR)/*.asm)
ASM_OBJECTS := $(patsubst $(ASM_DIR)/%.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

$(BUILD_DIR)/%.o: $(ASM_DIR)/%.asm
	mkdir -p $(BUILD_DIR)
	nasm -f elf64 $< -o $@

clean:
	rm -rf build/

build: $(ASM_OBJECTS)
	mkdir -p $(BUILD_DIR)
	cp $(CURDIR)/linker.ld $(BUILD_DIR)/
	
	$(eval LINKER_OBJS := $(foreach obj,$(ASM_OBJECTS),-z "$(obj)"))
	c3c build -z "-Tlinker.ld" $(LINKER_OBJS)