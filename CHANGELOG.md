# Changelog

All notable changes to Sinux are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [SemVer](https://semver.org/).

## [Unreleased]

## [0.6.0-alpha.1] - 2026-09-09

### Added
- `ARCHITECTURE.md`: full map of the tree, boot stages, memory layout,
  execution model, build/test documentation.
- `CONTRIBUTING.md`: setup, workflow, code style, Definition of Done.
- GitHub issue templates (bug report, feature request) and PR template.

## [0.5.0] - 2026-07-02

Baseline inherited from `SinuxProject/Sinux`.

### Added
- 64-bit kernel booting via GRUB Multiboot2 (BIOS + UEFI).
- PMM/VMM memory management, slab allocator.
- VFS with ramfs (`/`), procfs (`/proc`), ext2 (`/mnt/disk`).
- ATA PIO, PS/2 keyboard, serial, BGA framebuffer/TTY drivers.
- Linux-ABI-compatible syscall subset, round-robin scheduler, ELF64
  loader, ring-3 execution, pipes/signals primitives.
- In-kernel shell, minimal static libc, example `hello` program.

[Unreleased]: https://github.com/hesam-oxe/Sinux/compare/v0.6.0-alpha.1...HEAD
[0.6.0-alpha.1]: https://github.com/hesam-oxe/Sinux/releases/tag/v0.6.0-alpha.1
[0.5.0]: https://github.com/SinuxProject/Sinux/commit/6664367390147a59ebe8931f6329b6149f0b398e
