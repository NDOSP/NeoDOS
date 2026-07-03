APPS_DIR := ../build/APPS
CC := gcc
CFLAGS := -ffreestanding -O2 -fPIC -Wall -Wextra -nostdlib -nostartfiles \
          -fno-stack-protector -mno-red-zone \
          -MMD -MP -I../libs
LD := ld
LDFLAGS := -nostdlib -static -z separate-code -Ttext=0

SRCS := $(wildcard src/*.c)
APPS := $(patsubst src/%.c,%,$(SRCS))
BINS := $(addprefix $(APPS_DIR)/, $(APPS))

.PHONY: all clean

all: $(BINS)

$(APPS_DIR):
	@mkdir -p $(APPS_DIR)

$(APPS_DIR)/%.o: src/%.c | $(APPS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_DIR)/%: $(APPS_DIR)/%.o
	$(LD) $(LDFLAGS) $< -o $@
	rm -f $(APPS_DIR)/$*.o $(APPS_DIR)/$*.d

clean:
	rm -f $(APPS_DIR)/*.o $(APPS_DIR)/*.d $(APPS_DIR)/*
