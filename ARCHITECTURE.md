# Sinux Architecture

> Living document. Status: v0.5 baseline, Phase 0 (engineering infrastructure)
> in progress. See `CHANGELOG.md` and the project roadmap.

Sinux is a minimal 64-bit Unix-like kernel written from scratch in C11 and
x86_64 assembly. It boots via GRUB (Multiboot2) on BIOS and UEFI, provides
a Linux x86_64-ABI-compatible syscall subset, and runs ELF64 programs in
ring 3. It is **not** Linux and does not run Linux binaries.

---

## 1. Source tree

```
arch/x86_64/        Machine layer: boot.asm, GDT, IDT, PIC, PIT, syscall entry, linker.ld
boot/               GRUB config (grub.cfg)
drivers/            serial, tty, keyboard (PS/2), ATA PIO, PCI, framebuffer (fb/bga)
kernel/core/        kernel_main, boot stages, in-kernel shell, panic, multiboot2 parsing
kernel/proc/        process, round-robin scheduler, ELF loader, usermode switch, init spawn
kernel/fs/          VFS + ramfs (/) + procfs (/proc) + ext2 (/mnt/disk)
kernel/ipc/         pipes, signals (primitives)
kernel/syscall/     syscall dispatch table (Linux x86_64 ABI numbering, subset)
lib/                printk, kstring helpers, errno, io.h, sha256
mm/                 PMM page-frame allocator, VMM (4-level paging), slab allocator
userspace/libc/     minimal static libc (stdio/stdlib/string/unistd/signal/syscall)
userspace/hello/    example userspace program
userspace/init/     init source (present, not yet wired into the build)
scripts/            iso.mk (grub-mkrescue), qemu.mk (run targets), mkdisk.sh (ext2 image)
edk2/               OVMF firmware blob for UEFI runs
```

Each subsystem ships a `module.mk` that appends its objects to `$(OBJS)`;
the top-level `Makefile` links everything into `build/kernel.elf`.

---

## 2. Boot flow

```
BIOS/UEFI → GRUB → Multiboot2 → arch/x86_64/boot.asm (32-bit)
  1. validate Multiboot2 magic
  2. CPUID + long-mode check
  3. build PML4/PDPT/PDT (identity-map first 1 GiB)
  4. enable PAE + LME + paging, load 64-bit GDT
  5. jump to kernel_main() (64-bit C)
```

`kernel_main()` (`kernel/core/main.c`) proceeds in strict stages —
**order matters**, each stage depends on the previous one:

| Stage | What happens |
|-------|--------------|
| 1. Serial | `serial_init()` — `printk` works from here on (serial only) |
| 2. CPU + mem | `gdt_init`, `idt_init`, `pic_init`, `pit_init`, `keyboard_init`, `pmm_init`, `vmm_init` (IDT first: anything after may fault) |
| 3. Graphics | `bga_init` → discover real FB address from PCI BAR0 → `vmm_map_range` it → `fb_init` → `tty_init` |
| 4. Subsystems | `vfs_init`, `ramfs_mount("/")`, `procfs_mount("/proc")`, `tty_dev_init`, `proc_init`, `sched_init`, `syscall_init`, `ata_init` + `ext2_mount("/mnt/disk")`, create `/root /home /etc /tmp /bin /lib /usr` |
| 5. Go | `sti`, banner, `proc_spawn_init()` → idle loop; if no `/sbin/init` or `/bin/init` ELF exists, fall back to the in-kernel shell |

Console model: `printk` → `tty_putc` → **both** framebuffer and COM1
serial. Every boot marker below is therefore visible on the serial log,
which is what CI asserts on.

---

## 3. Memory layout

| Region | Address | Notes |
|--------|---------|-------|
| Low memory | `0x000000–0x0FFFFF` | BIOS/reserved |
| Kernel load (phys) | `0x100000` | 1 MiB (`KERNEL_PHYS_BASE`) |
| Kernel mapping | `0xFFFFFFFF80000000`, 64 MiB | `KERNEL_VIRT_BASE`, `KERNEL_MAP_SIZE` |
| User load base | `0x400000` | ELF default (`USER_LOAD_BASE`) |
| User stack | top `0x7FFFFFF00000`, 2 MiB | `USER_STACK_TOP/SIZE` |
| Page size | 4096 | `PAGE_SIZE`; 2 MiB huge-page flag defined (`VMM_HUGE`) |

- **PMM** (`mm/pmm.*`): page-frame allocator. API: `pmm_alloc/free`,
  `pmm_free_pages/total_pages`. No swap yet (Phase 1).
- **VMM** (`mm/vmm.*`): 4-level paging, per-process PML4
  (`vmm_new/clone/destroy_pml4`, `vmm_map/map_range/unmap`,
  `vmm_switch`). Upper-half entries shared into every address space.
- **Heap**: `kmalloc/kfree` (declared in `mm/vmm.h`) plus a separate
  slab layer (`mm/slab.*`, `kmalloc_slab/kfree_slab`).

---

## 4. Execution model

- **Single core**, preemptive ticks via PIT → `sched_tick()`.
- **Scheduler** (`kernel/proc/scheduler.*`): round-robin run queue
  (`sched_add/remove/yield/sleep/wake`).
- **Processes** (`kernel/proc/process.*`): PCB with own PML4 + kernel
  stack, fd table, cwd, brk. `fork` via `vmm_clone_pml4` (deep copy;
  no COW yet).
- **Ring 3 entry**: `build_user_stack` + `arch_switch` frame →
  `fork_child_stub` → `sysretq` (see `kernel/proc/init.c`,
  `arch/x86_64/boot.asm`).
- **Syscalls** (`kernel/syscall/`): `SYSCALL/SYSRET` enabled at boot
  (`syscall_init`); dispatch by Linux x86_64 syscall number (subset —
  authoritative list is the table in `kernel/syscall/syscall.c`).
- **Init**: PID 1 is spawned from `/sbin/init` (fallback `/bin/init`)
  ELF; fds 0/1/2 inherited from PID 0 (`/dev/tty0`).

## 5. Filesystems & drivers

- **VFS** (`kernel/fs/vfs.*`): `vfs_open/close/read/write/lookup/create/
  unlink/readdir`, mount table, per-FS `fs_ops`.
- **ramfs** `/` (volatile), **procfs** `/proc`, **ext2** `/mnt/disk`
  (persistent 128 MiB `sinux.img`, created by `scripts/mkdisk.sh`).
- **Drivers**: 16550 serial (COM1), PS/2 keyboard, VGA-text/TTY over
  BGA framebuffer, ATA PIO (`ata_get_drive`), PCI enumeration.
- Note: `kernel/core/auth.c` and `kernel/core/persist.c` exist in the
  tree but are **not** compiled in (see `kernel/core/module.mk`).

## 6. Build & run

| Command | Meaning |
|---------|---------|
| `make iso` | build kernel + bootable ISO + disk image (first time) |
| `make run-bios` / `run-uefi` | run in QEMU, graphical |
| `make run-serial` | headless, stdio on serial (used by tests) |
| `make run-debug` | QEMU interrupt log to `/tmp/sinux_qemu.log` |
| `make test` | headless boot + self-test assertion (added in v0.6.0-beta.1) |
| `make clean` | remove `build/` |

Dependencies: `nasm grub xorriso mtools qemu-system-x86 ovmf gcc binutils make`
(see `CONTRIBUTING.md` for per-distro install lines).

> ⚠️ Known build quirks (tracked as GitHub issues):
> 1. Bare `make` only builds `boot.o` — the first explicit target comes
>    from an included `module.mk`, shadowing `all`. Always invoke
>    `make iso` / `make all` / `make test` explicitly.
> 2. `make all` currently fails in `userspace/libc` (`stdarg.h` not found
>    under `-nostdinc`). The kernel + ISO path is unaffected.

## 7. Testing

- **Smoke test**: headless QEMU boot; assert serial contains the banner
  (`Sinux` / `Made By SUN`), the `RAM:` line, and the shell/idle marker;
  fail on `panic`. Implemented by `make test` + CI (Phase 0).
- **Kernel unit tests** (`kernel/test/`, from v0.6.0-beta.1): kunit-style
  asserts over serial for PMM and VFS; auto-run at boot, summary line
  `ktest: ALL TESTS PASSED (n/n)` or per-test `FAIL`.

## 8. Conventions

- K&R style, **tabs**, `-Wall -Wextra` clean, C11 freestanding.
- Kernel string/memory helpers are `k`-prefixed (`kstrlen`, `kstrcpy`,
  `kmemcpy`, …) in `lib/string.*`.
- Logging via `printk` with `KERN_*` levels (`lib/printk.h`).
- Commits follow Conventional Commits (`feat:`, `fix:`, `docs:`,
  `test:`, `ci:`, `refactor:` …). See `CONTRIBUTING.md`.
