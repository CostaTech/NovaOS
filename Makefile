# NovaOS build system
# Assembly + C + C++ freestanding kernel, booted by GRUB.

NASM ?= nasm
CC ?= gcc
CXX ?= g++
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -m32 -ffreestanding -fno-stack-protector -fno-builtin -fno-pic -Wall -Wextra -O2 -Iinclude
CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -nostdinc++
LDFLAGS := -m elf_i386 -T boot/linker.ld -nostdlib

BUILD := build
ISO_DIR := iso
KERNEL := $(BUILD)/NovaOS.kernel
ISO := NovaOS.iso

ASM_OBJS := $(BUILD)/entry.o
C_OBJS := $(BUILD)/vga.o $(BUILD)/ports.o $(BUILD)/keyboard.o $(BUILD)/mouse.o $(BUILD)/framebuffer.o
CPP_OBJS := $(BUILD)/main.o $(BUILD)/panic.o $(BUILD)/power.o $(BUILD)/desktop.o $(BUILD)/shell.o $(BUILD)/ramfs.o $(BUILD)/tenclelang.o
OBJS := $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS)

.PHONY: all iso run clean tree

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: boot/entry.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/%.o: drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: kernel/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: graphics/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: shell/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: fs/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: languages/tenclelang/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/NovaOS.kernel
	cp boot/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue --modules="part_msdos part_gpt iso9660 multiboot" -o $(ISO) $(ISO_DIR)
	@echo "Built $(ISO)"

run: iso
	qemu-system-i386 -m 256M -cdrom $(ISO) -boot d

clean:
	rm -rf $(BUILD) $(ISO) $(ISO_DIR)/boot/NovaOS.kernel

tree:
	find . -maxdepth 3 -type f | sort
