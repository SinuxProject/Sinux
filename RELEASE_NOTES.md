# Sinux 0.6.0 — Phase 0: Engineering Infrastructure ✅

**Status:** Phase 0 complete. `main` is green: every push builds the kernel
and ISO and proves the boot in headless QEMU with 21/21 self-test asserts.

This release adds **no new kernel features**. It lays the engineering
foundation everything else will stand on: documentation, CI, and tests —
plus the real bugs those tests exposed.

---

## What's inside

**Docs & process** (`v0.6.0-alpha.1`)
- `ARCHITECTURE.md`: tree map, boot stages, memory layout, build/test guide
- `CONTRIBUTING.md`: setup, workflow, code style, Definition of Done
- `CHANGELOG.md` (Keep a Changelog) + issue/PR templates

**CI/CD** (`v0.6.0-alpha.2`)
- GitHub Actions `build-and-test`: toolchain install → `make iso` →
  headless QEMU boot with serial-marker assertions; ISO + serial log
  uploaded as artifacts
- `scripts/smoke-test.sh`: reusable local/CI smoke script

**Self-tests** (`v0.6.0-beta.1`)
- In-kernel `ktest` framework (`kernel/test/`): suites auto-run at boot,
  report over serial; summary line `ktest: ALL TESTS PASSED`
- PMM suite: 11/11 (alloc/free, alignment, accounting)
- VFS suite: 10/10 (create/write/read/unlink round-trip on ramfs)
- `make test`: rebuild + headless boot + full assertion in one command

**Bugs the tests caught (all fixed, all covered)**
1. `split_path` broke every top-level `vfs_create`/`vfs_unlink` with
   `-EINVAL` — boot directories (`/root`, `/mnt`, …) never existed
2. PMM reserved ~2 MiB for a ~3 MiB kernel image — heap and bitmap
   overlapped `.bss` (also caused the bogus `RAM:` readout, now `251/255`)
3. Boot-time bump allocator overran single pages on multi-page requests
4. Slab header ate 4088 of 4096 bytes per page, spilling every object
   into the next physical page

## Verify it yourself

```bash
make iso    # build kernel + ISO
make test   # headless QEMU boot, expect: ktest: ALL TESTS PASSED (21/21)
```

CI must be green on every PR — see `CONTRIBUTING.md`.

## Known issues (tracked on GitHub)

- Bare `make` only builds `boot.o` (default goal shadowed by an included
  `module.mk`) — always invoke `make iso` / `make test` explicitly
- `make all` fails in `userspace/libc` (`stdarg.h` missing under
  `-nostdinc`) — kernel + ISO path is unaffected

## What's next — Phase 1

Modern x86_64 bring-up: LAPIC/x2APIC + IOAPIC, SMP, locking primitives,
preemptive fair scheduler, and full virtual memory (`mmap`, COW, swap).
See the project roadmap.
