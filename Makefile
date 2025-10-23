NASM=nasm
NASM_FLAGS=-f elf64

LD=ld
LD_FLAGS=--nmagic --script=$(LINKER)

GCC_VERSION := $(shell gcc-13 -dumpversion 2>/dev/null | cut -d. -f1)
ifneq ($(GCC_VERSION),13)
    GCC_VERSION := $(shell gcc -dumpversion 2>/dev/null | cut -d. -f1)
    ifeq ($(GCC_VERSION),13)
        CC=gcc
    else
        $(warning "GCC version 13 not found. Trying default gcc but interrupts may not work correctly.")
        CC=gcc
    endif
else
    CC=gcc-13
endif

CFLAGS=-Wall -c -ggdb -ffreestanding -mgeneral-regs-only

GRUB := $(shell which grub2-mkrescue 2>/dev/null || which grub-mkrescue 2>/dev/null)
ifeq ($(GRUB),)
    $(error "Neither grub2-mkrescue nor grub-mkrescue found. Please install GRUB tools.")
endif

GRUB_FLAGS=-o

LINKER=x86_64/boot/linker.ld

ISO_DIR=isofiles
ISO_BOOT_DIR := $(ISO_DIR)/boot

BUILD_DIR=build

x86_64_asm_source_files := $(shell find x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

kernel_source_files := $(shell find kernel -name *.c)
kernel_object_files := $(patsubst kernel/%.c, build/kernel/%.o, $(kernel_source_files))

kernel_asm_source_files := $(shell find kernel -name *.asm)
kernel_asm_object_files := $(patsubst kernel/%.asm, build/kernel/%.o, $(kernel_asm_source_files))

object_files := $(x86_64_asm_object_files) $(kernel_object_files) $(kernel_asm_object_files)

build/%.o: %.asm
	mkdir -p $(dir $@) && \
	$(NASM) $(NASM_FLAGS) $< -o $@

build/%.o: %.c
	mkdir -p $(dir $@) && \
	$(CC) $(CFLAGS) $< -o $@

$(ISO_BOOT_DIR)/kernel.bin: $(object_files)
	$(LD) $(LD_FLAGS) --output=$@ $^

$(ISO_DIR)/kernel.iso: $(ISO_BOOT_DIR)/kernel.bin
	$(GRUB) $(GRUB_FLAGS) $@ $(ISO_DIR)

build_kernel: $(ISO_BOOT_DIR)/kernel.bin
build_iso: $(ISO_DIR)/kernel.iso

QEMU=qemu-system-x86_64
QEMU_FLAGS=-m 128M -cdrom

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# -s -S -kernel
# tap adapter 

qemu: $(ISO_DIR)/kernel.iso
	$(QEMU)  \
	$(QEMU_FLAGS) \
	$(ISO_DIR)/kernel.iso

.gdbinit: .gdbinit.tmpl
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(ISO_DIR)/kernel.iso .gdbinit
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMU_FLAGS) $(ISO_DIR)/kernel.iso -S $(QEMUGDB)

clean: 
	rm -rf $(BUILD_DIR)
	rm -f $(ISO_DIR)/kernel.*
	rm -f $(ISO_DIR)/**/kernel.*

install:
	@if [ -f /etc/fedora-release ] || [ -f /etc/redhat-release ]; then \
		echo "Installing for Fedora/RHEL..."; \
		sudo dnf install -y xorriso mtools qemu-system-x86 grub2-tools; \
		sudo snap install gcc-13; \
	elif [ -f /etc/debian_version ] || [ -f /etc/lsb-release ]; then \
		echo "Installing for Debian/Ubuntu..."; \
		sudo apt update; \
		sudo apt install -y gcc-13 xorriso mtools qemu-system-x86 grub2-common grub-pc-bin; \
	elif [ -f /etc/arch-release ]; then \
		echo "Installing for Arch Linux..."; \
		sudo pacman -Syu --noconfirm && \
		sudo pacman -S --noconfirm gcc xorriso mtools qemu-system-x86 grub; \
	else \
		echo "Unknown distribution. Please install manually: gcc-13 (or gcc), xorriso, mtools, qemu-system-x86, grub2-mkrescue/grub-mkrescue"; \
	fi

check-deps:
	@echo "Checking dependencies..."; \
	echo "NASM: $(shell which nasm 2>/dev/null || echo 'NOT FOUND')"; \
	echo "GCC: $(CC) (version: $(GCC_VERSION))"; \
	echo "LD: $(shell which ld 2>/dev/null || echo 'NOT FOUND')"; \
	echo "GRUB: $(GRUB)"; \
	echo "QEMU: $(shell which qemu-system-x86_64 2>/dev/null || echo 'NOT FOUND')"