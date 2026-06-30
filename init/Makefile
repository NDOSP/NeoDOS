INIT_DIR := ../build
CC := gcc
CFLAGS := -ffreestanding -O2 -fPIC -Wall -Wextra -nostdlib -nostartfiles \
          -fno-stack-protector -mno-red-zone \
          -I../modules/include -MMD -MP
LD := ld
LDFLAGS := -nostdlib -static

.PHONY: all clean

all: $(INIT_DIR)/INIT.ELF

$(INIT_DIR)/INIT.ELF: src/init.c init.ld
	@mkdir -p $(INIT_DIR)
	$(CC) $(CFLAGS) -c src/init.c -o $(INIT_DIR)/init.o
	$(LD) $(LDFLAGS) -T init.ld $(INIT_DIR)/init.o -o $@

clean:
	rm -f $(INIT_DIR)/init.o $(INIT_DIR)/INIT.ELF
