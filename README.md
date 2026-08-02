# Sinux 1.0.0 MVP

A minimal 64-bit Unix-like operating system kernel written from scratch in C and x86_64 Assembly.
Boots via GRUB Multiboot2, supports UEFI and BIOS, and runs on real hardware or QEMU.

**Version:** 1.0.0 MVP (Minimum Viable Product)  
**Status:** Ready for development and testing  
**License:** Apache 2.0

---

## What is Sinux?

Sinux is a hobby operating system built to understand how real operating systems work at the lowest level.
It is not Linux. It does not run Linux programs. It is an independent kernel with its own:

- ✅ Physical and virtual memory manager
- ✅ VFS (Virtual File System) with ramfs and ext2 support
- ✅ ATA PIO disk driver for persistent storage
- ✅ PS/2 keyboard and VGA text mode drivers
- ✅ Serial (COM1) debug output
- ✅ IDT, GDT, TSS, PIC, PIT
- ✅ Syscall interface (Linux x86_64 ABI compatible)
- ✅ Process scheduler (round-robin)
- ✅ ELF64 loader for userspace programs
- ✅ Ring 3 (user mode) execution
- ✅ Pipe and signal primitives
- ✅ Built-in shell with filesystem commands
- ✅ Persistent storage (ext2 on ATA drive)
- ✅ Procfs for kernel information

---

## Architecture

```
sinux/
├── arch/x86_64/        Architecture-specific (GDT, IDT, PIC, PIT, syscall, boot)
├── kernel/
│   ├── core/           kernel_main, panic, multiboot2 parser
│   ├── proc/           Process management, ELF loader, scheduler, usermode
│   ├── fs/             VFS, ramfs, ext2, procfs
│   ├── ipc/            Pipes, signals
│   └── syscall/        Syscall dispatch table
├── mm/                 Physical (PMM) and virtual (VMM) memory managers
├── drivers/            VGA, serial, keyboard, TTY, ATA
├── lib/                string, printk, io.h
├── userspace/
│   ├── libc/           Minimal C library
│   └── hello/          Example userspace program
├── boot/               grub.cfg, linker.ld
└── scripts/            iso.mk, qemu.mk, mkdisk.sh
```

### Boot Flow

```
BIOS/UEFI → GRUB 2 → Multiboot2
  → boot.asm (32-bit)
      ├── Validate Multiboot2 magic
      ├── Check CPUID + Long Mode support
      ├── Build PML4/PDPT/PDT page tables (identity map first 1 GiB)
      ├── Enable PAE + LME + Paging
      ├── Load 64-bit GDT
      └── Jump to kernel_main() (64-bit C)
```

### Memory Layout

```
0x000000 - 0x0FFFFF    Low memory (BIOS, reserved)
0x100000               Kernel load address (1 MiB)
0x200000               PMM bitmap
[kernel BSS]           Page tables, kernel stack (32 KiB)
[above kernel]         Free physical pages (PMM managed)
/                      ramfs (in RAM, lost on reboot)
/mnt/disk              ext2 on ATA drive (persistent)
/proc                  procfs (kernel info)
```

### Syscall ABI

Follows the Linux x86_64 ABI:

| Register | Role          |
|----------|---------------|
| RAX      | syscall number |
| RDI      | arg 1         |
| RSI      | arg 2         |
| RDX      | arg 3         |
| R10      | arg 4         |
| R8       | arg 5         |
| R9       | arg 6         |
| RAX      | return value  |

Supported syscalls:

| Number | Name      | Description        |
|--------|-----------|--------------------|
| 0      | read      | Read from fd       |
| 1      | write     | Write to fd        |
| 2      | open      | Open file          |
| 3      | close     | Close fd           |
| 12     | brk       | Set heap end       |
| 22     | pipe      | Create pipe        |
| 39     | getpid    | Get process ID     |
| 57     | fork      | Fork process       |
| 59     | execve    | Execute ELF binary |
| 60     | exit      | Exit process       |
| 61     | wait4     | Wait for child     |
| 62     | kill      | Send signal        |
| 110    | getppid   | Get parent PID     |
| 162    | nanosleep | Sleep              |

---

## System Requirements

### Host (build machine)

| Component    | Minimum                            |
|--------------|------------------------------------|
| OS           | Any Linux (Arch, Ubuntu, Fedora…)  |
| Architecture | x86_64                             |
| RAM          | 512 MiB free                       |
| Disk         | 1 GiB free                         |

### Target (QEMU or real hardware)

| Component | Requirement                              |
|-----------|------------------------------------------|
| CPU       | x86_64 with Long Mode (post ~2003)       |
| RAM       | 64 MiB minimum, 256 MiB recommended      |
| Firmware  | BIOS or UEFI (Secure Boot must be OFF)   |
| Disk      | Optional ATA/IDE for persistent storage  |

---

## Installing Dependencies

### Arch Linux
```bash
sudo pacman -S --needed \
    nasm grub xorriso mtools \
    qemu-system-x86 edk2-ovmf \
    gcc binutils make
```

### Ubuntu / Debian
```bash
sudo apt install \
    nasm grub-pc-bin grub-efi-amd64-bin \
    xorriso mtools \
    qemu-system-x86 ovmf \
    gcc binutils make
```

### Fedora / RHEL
```bash
sudo dnf install \
    nasm grub2-tools xorriso mtools \
    qemu-system-x86 edk2-ovmf \
    gcc binutils make
```

---

## Building

```bash
# Build kernel + userspace
make

# Build bootable ISO and create disk image (first time only)
make iso

# Run in QEMU
make run-bios
```

### UEFI Mode

```bash
make run-uefi
```

### Make Targets

| Target           | Description                                  |
|------------------|----------------------------------------------|
| `make`           | Build kernel + userspace                     |
| `make iso`       | Build ISO + create sinux.img (first time)    |
| `make run-bios`  | Force BIOS mode                              |
| `make run-uefi`  | Force UEFI mode                              |
| `make run-serial`| Headless, output to terminal                 |
| `make run-debug` | Debug mode with QEMU interrupt log           |
| `make clean`     | Remove build artifacts                       |

---

## Shell Commands

```
ls [path]           List directory contents
cd <path>           Change directory  (cd .. supported)
pwd                 Print working directory
mkdir <path>        Create directory
touch <file>        Create empty file
rm <file>           Remove file
cat <file>          Print file contents
edit <file>         Line editor  (end with '.'  /  cancel with ':q')
write <file> <txt>  Write text to file (overwrites)
append <file> <txt> Append text to file
cp <src> <dst>      Copy file
mv <src> <dst>      Move / rename file
stat <file>         Show file information
mem                 Show memory map (Multiboot2)
memstat             Show memory usage
uptime              Show system uptime
uname               Show kernel version
cpuid               Show CPU info
clear               Clear screen
halt                Shutdown
```

---

## Persistent Storage

On first `make iso`, a 128 MiB ext2 disk image (`sinux.img`) is created automatically.
QEMU attaches it as a virtual ATA drive. The kernel mounts it at `/mnt/disk`.

```bash
# Files here persist across reboots
write /mnt/disk/notes.txt hello world
cp /etc/config /mnt/disk/config
```

`make run` never recreates `sinux.img` — your data is always safe.
To reset the disk: `rm sinux.img && make iso`

---

## Userspace Development

Sinux includes a minimal libc for writing real userspace programs.

### Build the libc

```bash
cd userspace/libc && make
```

### Write a program

```c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    printf("Hello from Sinux userspace!\n");
    printf("PID: %d\n", (int)getpid());
    return 0;
}
```

### Makefile for userspace programs

```makefile
CC      := gcc
CFLAGS  := -m64 -ffreestanding -nostdlib -nostdinc \
           -O2 -I../libc/include                   \
           -mno-red-zone -mno-sse -mno-sse2
LDFLAGS := -m elf_x86_64 -nostdlib -T linker.ld
LIBC    := ../libc/libc.a

myapp.elf: main.o $(LIBC)
	ld $(LDFLAGS) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<
```

### linker.ld for userspace

```ld
ENTRY(_start)
SECTIONS {
    . = 0x400000;
    .text   : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }
    .bss    : { *(COMMON) *(.bss .bss.*) }
}
```

### Available headers

| Header       | Contents                                      |
|--------------|-----------------------------------------------|
| `<stdio.h>`  | printf, puts, putchar, getchar                |
| `<stdlib.h>` | malloc, free, exit, atoi                      |
| `<string.h>` | strlen, strcmp, strcpy, memset, memcpy        |
| `<unistd.h>` | read, write, close, getpid, fork, execve, pipe|
| `<signal.h>` | signal, SIGINT, SIGKILL, SIGTERM              |
| `<syscall.h>`| raw syscall numbers and syscall()             |

---

## Running on Real Hardware

```bash
lsblk
sudo dd if=build/iso/sinux.iso of=/dev/sdX bs=4M status=progress
sync
```

1. Reboot into BIOS/UEFI (F2, F12, Del or Esc)
2. **Disable Secure Boot**
3. Set boot order to USB first
4. Save and reboot

---

## Debugging

```bash
make run-debug
cat /tmp/sinux_qemu.log | grep -E "exception|fault|INT" | tail -20

make run-serial
```

---

## Known Limitations (Roadmap for 2.0.0)

The following features are planned for future versions:

- ❌ Single core only (no SMP) - *Planned: Multi-core support*
- ❌ No networking - *Planned: Ethernet driver (e1000, virtio-net)*
- ❌ No USB driver (PS/2 keyboard only) - *Planned: UHCI/EHCI support*
- ❌ No ACPI (halt stops CPU, does not power off) - *Planned: ACPI AML interpreter*
- ❌ ext2 is basic (no journaling, no fsck) - *Planned: ext3/ext4 support*
- ❌ No dynamic linking - *Planned: ELF dynamic loader*
- ❌ No virtual memory swapping - *Planned: Swap space support*
- ❌ No graphical display (VGA text mode only) - *Planned: Framebuffer, GUI*
- ❌ No sound support - *Planned: AC97, HDA drivers*
- ❌ Limited shell features - *Planned: Scripting, pipes, redirections*

---

## Version History

### 1.0.0 MVP (Current)
- ✅ Complete memory management (PMM + VMM)
- ✅ Full VFS with ramfs, ext2, and procfs
- ✅ Process scheduler with round-robin
- ✅ Complete syscall interface (Linux ABI compatible)
- ✅ Userspace libc and program loader
- ✅ Persistent storage via ATA PIO
- ✅ Interactive shell with 20+ commands
- ✅ PS/2 keyboard and VGA text drivers
- ✅ Serial debugging output
- ✅ Multiboot2 (BIOS + UEFI) support

### 0.5.0 (Previous)
- Initial kernel structure
- Basic memory management
- Simple VFS implementation
- Minimal process management
- Basic shell commands

---

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test thoroughly in QEMU and/or real hardware
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

### Development Guidelines

- Follow the existing code style (K&R with tabs)
- Add comments for complex logic
- Test on both BIOS and UEFI modes
- Ensure no memory leaks in kernel code
- Keep userspace programs minimal and focused

---

## Support & Community

- **Issues:** Report bugs and feature requests on GitHub Issues
- **Discussions:** Join discussions about development and ideas
- **Wiki:** Check the wiki for detailed documentation (coming soon)

---

## Acknowledgments

This project was inspired by:
- OSDev Wiki (wiki.osdev.org)
- Linux Kernel source code
- xv6 from MIT
- Various hobby OS projects

Special thanks to the OSDev community for their invaluable resources and support.

---

## License

Apache 2.0

---

**Sinux 1.0.0 MVP - A stepping stone towards understanding operating systems at the deepest level.**
