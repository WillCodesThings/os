CC := x86_64-elf-gcc
LD := x86_64-elf-ld
AS := nasm

BUILD ?= debug

SRC_KERNEL := src/impl/kernel
SRC_ARCH   := src/impl/x86_64

BUILD_DIR  := build
DIST_DIR   := dist/x86_64

ISO_DIR    := targets/x86_64/iso
LINKER     := targets/x86_64/linker.ld

INCLUDES   := -I src/intf

CFLAGS := -ffreestanding $(INCLUDES) \
          -Wall -Wextra \
          -Werror=implicit-function-declaration \
          -Werror=incompatible-pointer-types \
          -Werror=int-conversion \
          -MMD -MP

ifeq ($(BUILD),debug)
    CFLAGS += -O0 -g -DDEBUG
else
    CFLAGS += -O2
endif

KERNEL_C_SRC := $(shell find $(SRC_KERNEL) -name '*.c')
ARCH_C_SRC   := $(shell find $(SRC_ARCH)   -name '*.c')
ARCH_ASM_SRC := $(shell find $(SRC_ARCH)   -name '*.asm')

KERNEL_OBJ := $(patsubst $(SRC_KERNEL)/%.c, $(BUILD_DIR)/kernel/%.o, $(KERNEL_C_SRC))
ARCH_C_OBJ := $(patsubst $(SRC_ARCH)/%.c,   $(BUILD_DIR)/x86_64/%.o, $(ARCH_C_SRC))
ARCH_ASM_OBJ := $(patsubst $(SRC_ARCH)/%.asm, $(BUILD_DIR)/x86_64/%.o, $(ARCH_ASM_SRC))

OBJS := $(KERNEL_OBJ) $(ARCH_C_OBJ) $(ARCH_ASM_OBJ)


.PHONY: all build-x86_64 clean check

all: build-x86_64

# --- Kernel C files ---
$(BUILD_DIR)/kernel/%.o: $(SRC_KERNEL)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

# --- Arch-specific C files ---
$(BUILD_DIR)/x86_64/%.o: $(SRC_ARCH)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

# --- Assembly files ---
$(BUILD_DIR)/x86_64/%.o: $(SRC_ARCH)/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

build-x86_64: $(OBJS)
	@mkdir -p $(DIST_DIR)
	$(LD) -n -o $(DIST_DIR)/kernel.bin -T $(LINKER) $(OBJS)
	cp $(DIST_DIR)/kernel.bin $(ISO_DIR)/boot/kernel.bin
	grub-mkrescue /usr/lib/grub/i386-pc -o $(DIST_DIR)/kernel.iso $(ISO_DIR)

check:
	@echo "Checking for implicit declarations..."
	@! grep -R "implicit declaration" $(BUILD_DIR) || false

clean:
	rm -rf $(BUILD_DIR) dist

-include $(KERNEL_OBJ:.o=.d)
-include $(ARCH_C_OBJ:.o=.d)
