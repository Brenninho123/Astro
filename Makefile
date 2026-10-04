# Astro OS - build
#
#   make kernel   bare-metal x86 kernel  -> build/astro.elf
#   make sim      Windows simulator (GUI) -> build/sim/astro-sim.exe
#   make preview  render home-screen frames to build/preview/shots
#   make android  Android app             -> build/astro.apk (needs the Android SDK + NDK)
#   make run      run the kernel in QEMU
#   make iso      bootable ISO (needs grub-mkrescue and xorriso)
#
# Kernel toolchain: the i686-elf-gcc cross-compiler by default. On Linux/WSL the
# native gcc also works with:  make CROSS= kernel   (needs gcc-multilib).
#
# Simulator toolchain: MinGW-w64 (MSYS2 on Windows). To cross-compile it from Linux:
#   make sim SIM_CC=x86_64-w64-mingw32-gcc SIM_WINDRES=x86_64-w64-mingw32-windres

BUILD    := build
INCLUDES := -Isrc/kernel -Isrc/apps

.PHONY: all kernel sim preview android run iso run-iso clean

all: kernel

# ---------------------------------------------------------------------------
# Bare-metal x86 kernel
# ---------------------------------------------------------------------------

CROSS ?= i686-elf-
CC     = $(CROSS)gcc

ARCH_FLAGS :=
ifeq ($(CROSS),)
ARCH_FLAGS := -m32
endif

KERNEL := $(BUILD)/astro.elf
ISO    := $(BUILD)/astro.iso

CFLAGS  := $(ARCH_FLAGS) -std=gnu11 -O2 -Wall -Wextra $(INCLUDES) -Isrc/arch/x86 \
           -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
           -fno-asynchronous-unwind-tables -fcf-protection=none
LDFLAGS := $(ARCH_FLAGS) -T linker.ld -nostdlib -static -no-pie \
           -Wl,-z,noexecstack -Wl,--build-id=none

X86_C_SRCS := $(wildcard src/kernel/*.c) $(wildcard src/arch/x86/*.c) $(wildcard src/apps/*.c)
X86_S_SRCS := $(wildcard src/boot/*.S)
X86_C_OBJS := $(patsubst src/%.c,$(BUILD)/x86/%.o,$(X86_C_SRCS))
X86_S_OBJS := $(patsubst src/%.S,$(BUILD)/x86/%.o,$(X86_S_SRCS))
X86_OBJS   := $(X86_C_OBJS) $(X86_S_OBJS)

kernel: $(KERNEL)

$(KERNEL): $(X86_OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(X86_OBJS)

$(X86_C_OBJS): $(BUILD)/x86/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(X86_S_OBJS): $(BUILD)/x86/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) -c $< -o $@

# QEMU understands Multiboot directly, no GRUB needed.
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

iso: $(KERNEL)
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/astro.elf
	printf 'set timeout=0\nmenuentry "Astro OS" {\n\tmultiboot /boot/astro.elf\n\tboot\n}\n' > $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(BUILD)/iso

run-iso: iso
	qemu-system-i386 -cdrom $(ISO)

# ---------------------------------------------------------------------------
# Windows simulator (the graphical home screen in a normal Win32 window)
# ---------------------------------------------------------------------------

SIM_CC      ?= gcc
SIM_WINDRES ?= windres

SIM_EXE := $(BUILD)/sim/astro-sim.exe
SIM_RES := $(BUILD)/sim/astro_rc.o

SIM_CFLAGS  := -std=gnu11 -O2 -Wall -Wextra -DASTRO_HOSTED -Isrc/kernel -Isrc/desktop -Isrc/sim
SIM_LDFLAGS := -static -mwindows
SIM_LIBS    := -lm

# kstring.c is the freestanding libc replacement: the simulator uses the real C runtime.
SIM_C_SRCS := $(filter-out src/kernel/kstring.c,$(wildcard src/kernel/*.c)) \
              $(wildcard src/desktop/*.c) $(wildcard src/sim/*.c)
SIM_OBJS   := $(patsubst src/%.c,$(BUILD)/sim/%.o,$(SIM_C_SRCS))

sim: $(SIM_EXE)

$(SIM_EXE): $(SIM_OBJS) $(SIM_RES)
	$(SIM_CC) $(SIM_LDFLAGS) -o $@ $(SIM_OBJS) $(SIM_RES) $(SIM_LIBS)

$(SIM_OBJS): $(BUILD)/sim/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(SIM_CC) $(SIM_CFLAGS) -MMD -MP -c $< -o $@

# Embeds the icon and version info. assets/astro.ico comes from tools/make-icon.ps1.
$(SIM_RES): src/sim/astro.rc assets/astro.ico
	@mkdir -p $(dir $@)
	$(SIM_WINDRES) -I. -O coff $< -o $@

# Renders frames of the home screen (desktop and phone layouts) to build/preview/shots.
PREVIEW_SRCS := tools/preview.c src/desktop/desktop.c src/desktop/gfx.c \
                src/kernel/kprintf.c src/kernel/shell.c src/kernel/vga.c src/kernel/bootlog.c

preview:
	@mkdir -p $(BUILD)/preview/shots/desktop $(BUILD)/preview/shots/phone
	$(SIM_CC) -std=gnu11 -O2 -Wall -Wextra -DASTRO_HOSTED -Isrc/kernel -Isrc/desktop \
	    $(PREVIEW_SRCS) -lm -o $(BUILD)/preview/preview
	$(BUILD)/preview/preview $(BUILD)/preview/shots/desktop 1024 640
	$(BUILD)/preview/preview $(BUILD)/preview/shots/phone 720 1500 touch

# ---------------------------------------------------------------------------
# Android app (see android/build-apk.sh)
# ---------------------------------------------------------------------------

android:
	bash android/build-apk.sh

# ---------------------------------------------------------------------------

clean:
	rm -rf $(BUILD)

-include $(X86_OBJS:.o=.d) $(SIM_OBJS:.o=.d)
