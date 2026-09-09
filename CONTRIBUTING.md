# Contributing to Sinux

Thanks for helping build Sinux! This guide covers setup, workflow, and the
rules that keep `main` green.

## 1. Prerequisites

You need a Linux x86_64 host with:

```
nasm grub xorriso mtools qemu-system-x86 ovmf gcc binutils make e2fsprogs
```

Install lines:

```bash
# Debian / Ubuntu
sudo apt install nasm grub-pc-bin grub-efi-amd64-bin xorriso mtools \
    qemu-system-x86 ovmf gcc binutils make e2fsprogs

# Arch
sudo pacman -S --needed nasm grub xorriso mtools qemu-system-x86 \
    edk2-ovmf gcc binutils make e2fsprogs

# Fedora
sudo dnf install nasm grub2-tools xorriso mtools qemu-system-x86 \
    edk2-ovmf gcc binutils make e2fsprogs
```

## 2. Build, run, test

```bash
make iso          # build kernel + ISO (+ disk image on first run)
make run-serial   # headless QEMU run (Ctrl-A X to quit)
make test         # headless boot + self-test assertion (must pass)
make clean        # remove build artifacts
```

> Note: bare `make` only builds `boot.o` (known quirk, see
> `ARCHITECTURE.md` §6). Always use an explicit target.

CI runs `make iso` and `make test` on Ubuntu for every PR. **A PR is only
mergeable when CI is green.**

## 3. Workflow

1. Fork the repo, create a branch from `main`: `feature/<name>`,
   `fix/<name>`, `docs/<name>`, …
2. Keep commits **atomic**: one logical change per commit. Never mix
   features, refactors, and formatting in one commit.
3. Use **Conventional Commits**: `feat:`, `fix:`, `docs:`, `test:`,
   `ci:`, `refactor:`, `perf:`, `chore:`.
4. Before pushing: `make iso` **and** `make test` must pass locally.
5. Open a PR against `main` using the template. Small PRs review faster.

## 4. Definition of Done (every PR)

- [ ] Code builds warning-free (`-Wall -Wextra`)
- [ ] `make test` passes locally
- [ ] New behavior has an automated test (kunit self-test or QEMU assertion)
- [ ] Docs updated (`ARCHITECTURE.md` / `README.md` / `CHANGELOG.md` as needed)
- [ ] No dead code, no commented-out blocks, no TODO without an issue link

## 5. Code style

- K&R braces, **tabs for indentation**, C11, freestanding (`-ffreestanding`,
  `-nostdlib`, no red zone, no SSE in kernel).
- Kernel helpers are `k`-prefixed (`kstrlen`, `kmemcpy`, …).
- Log with `printk` + `KERN_*` levels; log boot problems loudly.
- Comment the *why* of tricky logic (paging, context switch, locking);
  keep comments in English.

## 6. Reporting bugs & security issues

- Bugs: open an issue with the **Bug report** template (host OS, QEMU
  version, serial log attached).
- Security vulnerabilities: **do not** open a public issue — contact the
  maintainers privately (see `README.md` / repo contact).

## 7. Releases

Releases are cut by maintainers from `main` with annotated tags
(`vX.Y.Z`, plus `-alpha/-beta/-rc` pre-releases). Every release updates
`CHANGELOG.md` and ships `RELEASE_NOTES.md`.
