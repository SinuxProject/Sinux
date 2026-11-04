CC     := gcc
AS     := nasm
AR     := ar
GCC_INC := $(shell $(CC) -print-file-name=include)
CFLAGS := -m64 -ffreestanding -nostdlib -nostdinc \
          -O2 -Wall -Iinclude -I$(GCC_INC) \
          -mno-red-zone -mno-sse -mno-sse2 \
          -fno-stack-protector
ASFLAGS := -f elf64

LIB  := libc.a
OBJS := src/start.o src/syscall.o src/syscall_wrap.o \
        src/stdio.o src/stdlib.o src/string.o

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/start.o: src/start.asm
	$(AS) $(ASFLAGS) -o $@ $<

src/syscall.o: src/syscall.asm
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(LIB)
