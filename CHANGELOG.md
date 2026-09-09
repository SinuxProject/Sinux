# Changelog

All notable changes to Sinux are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [SemVer](https://semver.org/).

## [Unreleased]

## [0.6.0] - 2026-09-09

Phase 0 (engineering infrastructure) is complete: documented, CI-gated,
self-tested baseline. No new kernel features. Details in `RELEASE_NOTES.md`
and in the `0.6.0-alpha.1`, `0.6.0-alpha.2`, `0.6.0-beta.1` entries below.

## [0.6.0-beta.1] - 2026-09-09

### Added
- In-kernel unit-test framework (`kernel/test/`): `KTEST_ASSERT`
  suites reporting over serial, auto-run at boot.
- Self-test suites for the PMM (alloc/free, alignment, accounting)
  and VFS (create/write/read/unlink round-trip on ramfs).
- `make test` target: rebuilds the ISO and asserts the headless QEMU
  boot, including `ktest: ALL TESTS PASSED`. CI now runs `make test`.

### Fixed
- `split_path` aliased the name buffer for top-level paths (`/foo`), so
  every top-level `vfs_create`/`vfs_unlink` failed with `-EINVAL`.
  Boot-time directories (`/root`, `/mnt`, …) are now actually created.
- The PMM reserved only a hardcoded ~2 MiB for the kernel image while
  the image (large `.bss`) spans ~3 MiB, so heap pages and the PMM
  bitmap overlapped kernel memory. The PMM now reserves everything up
  to the linker-provided `_kernel_end` and places its bitmap above it.
  This also fixes the bogus `RAM:` free-memory readout.
- The boot-time bump allocator in `kmalloc` claimed `pages * PAGE_SIZE`
  bytes while owning a single page. Refills now require physically
  contiguous pages and fail cleanly otherwise.
- The slab header's oversized free-bitmap left 8 usable bytes per page,
  pushing every slab object past the page end into the next physical
  page. The header is now 56 bytes with a 256-object bitmap cap.

## [0.6.0-alpha.2] - 2026-09-09

### Added
- GitHub Actions workflow `build-and-test`: installs the toolchain,
  runs `make iso`, boots the ISO headless in QEMU and asserts serial
  markers. Uploads the ISO and serial log as artifacts.
- `scripts/smoke-test.sh`: reusable headless boot/smoke-test script.

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

[Unreleased]: https://github.com/hesam-oxe/Sinux/compare/v0.6.0...HEAD
[0.6.0]: https://github.com/hesam-oxe/Sinux/releases/tag/v0.6.0
[0.6.0-beta.1]: https://github.com/hesam-oxe/Sinux/releases/tag/v0.6.0-beta.1
[0.6.0-alpha.2]: https://github.com/hesam-oxe/Sinux/releases/tag/v0.6.0-alpha.2
[0.6.0-alpha.1]: https://github.com/hesam-oxe/Sinux/releases/tag/v0.6.0-alpha.1
[0.5.0]: https://github.com/SinuxProject/Sinux/commit/6664367390147a59ebe8931f6329b6149f0b398e
