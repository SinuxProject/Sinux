# Sinux v0.6.0 — Phase 0 Complete: Release Announcement

**Sinux 0.6.0 is here: the first fully documented, CI-gated, and self-tested
baseline in project history — and the foundation for everything that follows.**

---

## What v0.6.0 delivers

**Engineering infrastructure (the whole point of Phase 0)**
- Complete CI/CD: every push builds the kernel + ISO and proves the boot in
  headless QEMU, asserting serial markers (`build-and-test` workflow, green)
- In-kernel self-test framework (`ktest`): **21/21 asserts passing**
  (PMM 11/11, VFS 10/10), auto-run at every boot
- One-command verification: `make test`
- Full architecture documentation (`ARCHITECTURE.md`), contribution guide,
  changelog discipline, and issue/PR templates

**Four critical bugs found and fixed by the new tests**
1. `split_path` rejected every top-level path — boot directories never existed
2. PMM reservation overlapped kernel `.bss` with heap and bitmap pages
   (this also repairs the free-memory readout: `RAM: 251/255 MiB`)
3. Boot-time bump allocator overran single pages on multi-page requests
4. Slab header consumed the entire page, spilling objects into adjacent pages

See `RELEASE_NOTES.md` and `CHANGELOG.md` for details.

## Stability statement

v0.6.0 is the most rigorously verified point in Sinux history: documented,
reproducibly built, and green in CI on every commit. It establishes the
engineering foundation for all future development — including future
commercial and industrial use as the kernel matures through the roadmap
phases (SMP, networking, self-hosting, hardening).

## Verify it yourself

```bash
git checkout v0.6.0
make iso && make test
# expect: ktest: ALL TESTS PASSED (2/2 suites, 21/21 asserts)
```

## What's next

Phase 1 — modern platform bring-up: APIC, SMP, locking, a preemptive
scheduler, and full virtual memory. Contributions welcome (see
`CONTRIBUTING.md`).

---
*Released 2026-09-09 · Tags: v0.6.0-alpha.1 → v0.6.0-alpha.2 →
v0.6.0-beta.1 → v0.6.0 · License: Apache-2.0*
