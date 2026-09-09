ARCH   := x86_64
BUILD  := build
TARGET := $(BUILD)/kernel.elf

CC ?= cc
AS  := nasm
LD ?= ld

ifeq ($(CC),clang)
CFLAGS_ARCH := --target=x86_64-elf
LD_CMD      := ld.lld
else
CFLAGS_ARCH := -m64
LD_CMD      := $(LD)
endif

CFLAGS := $(CFLAGS_ARCH)        \
    -ffreestanding -O2           \
    -Wall -Wextra                \
    -mno-red-zone                \
    -mno-mmx -mno-sse -mno-sse2  \
    -fno-stack-protector         \
    -fno-builtin -nostdlib       \
    -std=c11 -I.

ASFLAGS := -f elf64
LDFLAGS := -n -T arch/$(ARCH)/linker.ld --nostdlib

OBJS :=
include arch/$(ARCH)/module.mk
include lib/module.mk
include drivers/module.mk
include mm/module.mk
include kernel/core/module.mk
include kernel/ipc/module.mk
include kernel/proc/module.mk
include kernel/fs/module.mk
include kernel/syscall/module.mk
include kernel/test/module.mk

.PHONY: all iso run run-uefi run-bios run-serial test clean deps userspace disk

all: $(TARGET) userspace

userspace:
	$(MAKE) -C userspace/libc
	$(MAKE) -C userspace/hello

$(TARGET): $(OBJS) arch/$(ARCH)/linker.ld | $(BUILD)
	$(LD_CMD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[OK] $(TARGET)"

$(BUILD):
	mkdir -p $@

include scripts/iso.mk
include scripts/qemu.mk

test: iso
	SMOKE_TIMEOUT=120 ./scripts/smoke-test.sh

clean:
	rm -rf $(BUILD)
	$(MAKE) -C userspace/libc clean
	$(MAKE) -C userspace/hello clean
	@echo "[OK] Cleaned"
