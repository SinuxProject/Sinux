## What this PR contains

1. **Build/boot fixes for `main`** (`sinux-build-fixes.patch`) — `.DEFAULT_GOAL`,
   SMP include paths, `.code32`/`.code64` and IPI vector fixes, freestanding
   `stdarg.h`, `linker.ld` ASSERT/trampoline placement, duplicate `sched.o`,
   and disabling AP bring-up to match the upstream "single core only" note.
2. **`installer/` — Sinux Installer 1.1.0**: terminal-only live ISO,
   BIOS+UEFI, EN/FA, Multiboot2, GPT (+ BIOS Boot Partition + ESP), PID 1
   supervisor, `console=tty0` VGA fix, and post-install boot handover
   (auto reboot, ISO GRUB detects the installed system, UEFI `BootNext`).

## Verification

`installer/verification-1.1.0.log`: 74 passed / 0 failed across T1–T16.
`installer/evidence/*.png` are raw QEMU QMP framebuffer dumps showing the
GRUB menu and the installer UI on plain VGA. `make all -j4` and 21/21 kernel
self-tests pass after the fixes.

ISO (not committed, 66 MB) SHA256:
`aa416b0e2569d4275f1778c8dac05bb3df04bc2be4725e2f2ae9837d5eb78535`

Signed-off-by: AM <Araz0-0dev@users.noreply.github.com>
