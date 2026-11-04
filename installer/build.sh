#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Sinux Installer ISO build script 1.1.0 (reproducible)
#
# Produces a hybrid BIOS+UEFI bootable ISO that boots a tiny terminal-only
# Linux live system and runs a CLI/TUI installer for the Sinux kernel.
#
# Usage: ./build.sh [--clean]
#   --clean   remove the heavy build cache (.deb, extracted modules, rootfs)
#             after a successful build
# ---------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HERE/.build}"
DIST="${DIST:-$HERE/../release}"
SINUX_SRC="${SINUX_SRC:-$HERE/../Sinux}"
SRC="$HERE/src"
VERSION=1.1.0
ISO_NAME="Sinux-Installer-${VERSION}-x86_64.iso"
CLEAN=0
[ "${1:-}" = "--clean" ] && CLEAN=1

ROOTFS="$WORK/rootfs"
STAGE="$WORK/stage"
KDIR="$WORK/kernel"
mkdir -p "$WORK" "$DIST" "$KDIR" "$ROOTFS" "$STAGE"

echo "==> [1/10] Making sure upstream Sinux source is patched and built"
# The upstream clone is intentionally NOT shipped in the workspace: it can be
# re-downloaded from GitHub at any time.  Fail with an actionable hint instead
# of an obscure "No such file or directory".
if [ ! -f "$SINUX_SRC/Makefile" ]; then
    echo "ERROR: upstream Sinux source missing at $SINUX_SRC" >&2
    echo "       git clone --depth 1 https://github.com/SinuxProject/Sinux.git \"$SINUX_SRC\"" >&2
    echo "       (or set SINUX_SRC=/path/to/Sinux)" >&2
    exit 1
fi
if ! grep -q '^\.DEFAULT_GOAL' "$SINUX_SRC/Makefile" 2>/dev/null; then
    echo "    applying patches/sinux-build-fixes.patch"
    (cd "$SINUX_SRC" && git apply --ignore-whitespace "$HERE/patches/sinux-build-fixes.patch")
fi
# Build both the kernel AND the userspace sample (hello.elf) so the payload
# cannot pick up a stale binary from an older run.
make -C "$SINUX_SRC" all -j"$(nproc)" >/dev/null
KERNEL_ELF="$SINUX_SRC/build/kernel.elf"
HELLO_ELF="$SINUX_SRC/userspace/hello/hello.elf"
[ -f "$KERNEL_ELF" ] || { echo "kernel.elf missing"; exit 1; }
echo "    kernel.elf: $(stat -c '%s bytes' "$KERNEL_ELF")"
echo "    hello.elf : $(stat -c '%s bytes' "$HELLO_ELF" 2>/dev/null || echo missing)"

echo "==> [2/10] Obtaining a Debian x86_64 kernel (vmlinuz + modules)"
if ! ls "$KDIR"/*.deb >/dev/null 2>&1; then
    pkg=$(apt-cache depends linux-image-amd64 | awk '/Depends: linux-image-/{print $2; exit}')
    echo "    downloading $pkg"
    (cd "$KDIR" && apt-get download "$pkg")
fi
DEB=$(ls "$KDIR"/*.deb | head -n 1)
rm -rf "$KDIR/extracted"
mkdir -p "$KDIR/extracted"
dpkg-deb -x "$DEB" "$KDIR/extracted"
MODROOT=$(find "$KDIR/extracted/usr/lib/modules" -mindepth 1 -maxdepth 1 -type d | head -n 1)
KVER=$(basename "$MODROOT")
VMLINUZ=$(find "$KDIR/extracted/boot" -name 'vmlinuz-*' | head -n 1)
echo "    kernel version: $KVER"
echo "    vmlinuz: $(stat -c '%s bytes' "$VMLINUZ")"

echo "==> [3/10] Preparing the initramfs rootfs skeleton"
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,sbin,etc,proc,sys,dev,run,tmp,var/log,mnt/target,sinux,usr,lib,lib64}
cp /usr/bin/busybox "$ROOTFS/bin/busybox"
# busybox --install writes ABSOLUTE symlinks pointing at /usr/bin/busybox.
# Provide that path inside the rootfs too, otherwise /bin/sh (the /init
# interpreter) is dangling and the kernel panics with "error -2".
mkdir -p "$ROOTFS/usr/bin"
ln -sf /bin/busybox "$ROOTFS/usr/bin/busybox"
busybox --install -s "$ROOTFS/bin"
ln -sf /usr/bin/bash "$ROOTFS/bin/bash"
# On Debian 13 modprobe/depmod live in /usr/sbin as symlinks to kmod.
# Recreate them in /usr/bin too so /proc/sys/kernel/modprobe and the /sbin
# compatibility links are never dangling (they were in 1.0.0).
mkdir -p "$ROOTFS/usr/bin" "$ROOTFS/sbin"
ln -sf kmod "$ROOTFS/usr/bin/modprobe"
ln -sf kmod "$ROOTFS/usr/bin/depmod"
ln -sf /usr/bin/modprobe "$ROOTFS/sbin/modprobe"
ln -sf /usr/bin/depmod   "$ROOTFS/sbin/depmod"
ln -sf /usr/sbin/blkid   "$ROOTFS/usr/bin/blkid"

echo "==> [4/10] Copying binaries and shared libraries"
BINLIST=(
    /bin/bash /usr/bin/dialog /usr/bin/setfont /usr/bin/gettext
    /usr/sbin/sfdisk /usr/sbin/mke2fs /usr/sbin/mkfs.fat /usr/sbin/partprobe
    /usr/sbin/wipefs /usr/sbin/blockdev /usr/sbin/grub-install /usr/sbin/grub-probe
    /usr/bin/lsblk /usr/sbin/blkid /usr/bin/partx /usr/bin/findmnt /usr/bin/efibootmgr
    /usr/bin/mount /usr/bin/umount /usr/bin/kmod
    /usr/bin/sed /usr/bin/grep /usr/bin/find /usr/bin/sort /usr/bin/uniq
    /usr/bin/cut /usr/bin/head /usr/bin/tail /usr/bin/tr /usr/bin/wc
    /usr/bin/stat /usr/bin/readlink /usr/bin/realpath /usr/bin/mktemp
    /usr/bin/dirname /usr/bin/basename /usr/bin/cp /usr/bin/mv /usr/bin/rm
    /usr/bin/mkdir /usr/bin/chmod /usr/bin/ln /usr/bin/dd /usr/bin/sync
    /usr/bin/expr /usr/bin/uname /usr/bin/ls /usr/bin/cat /usr/bin/echo
    /usr/bin/printf /usr/bin/test /usr/bin/true /usr/bin/false
    /usr/bin/date /usr/bin/sleep /usr/bin/tee /usr/bin/env /usr/bin/pwd
    /usr/bin/install /usr/bin/awk /usr/bin/od /usr/bin/cmp /usr/bin/xargs
    /usr/bin/getopt /usr/bin/seq /usr/bin/id /usr/bin/iconv
)
for f in /usr/bin/grub-* /usr/sbin/grub-*; do [ -e "$f" ] && BINLIST+=("$f"); done
EXIST=()
for f in "${BINLIST[@]}"; do [ -e "$f" ] && EXIST+=("$f"); done
python3 "$SRC/collect_libs.py" "$ROOTFS" "${EXIST[@]}"

# On Debian (usrmerge) /lib and /usr/lib are the same directory, but our
# rootfs keeps /lib as a real directory because /lib/modules lives there.
# Relative SONAME symlinks such as libtinfo.so.6 -> libtinfo.so.6.5 would
# otherwise dangle. Mirror the multiarch library directory into /lib.
mkdir -p "$ROOTFS/lib/x86_64-linux-gnu"
# Hard-link instead of copy: GNU cpio stores a hardlinked file only once,
# saving ~11 MiB of initramfs and ~11 MiB of RAM during unpacking.
cp -alf "$ROOTFS/usr/lib/x86_64-linux-gnu"/. "$ROOTFS/lib/x86_64-linux-gnu"/ 2>/dev/null || true

# mkfs.* symlinks (mke2fs / mkfs.fat are the real binaries)
mkdir -p "$ROOTFS/usr/sbin"
ln -sf mke2fs   "$ROOTFS/usr/sbin/mkfs.ext2"
ln -sf mke2fs   "$ROOTFS/usr/sbin/mkfs.ext3"
ln -sf mke2fs   "$ROOTFS/usr/sbin/mkfs.ext4"
ln -sf mkfs.fat "$ROOTFS/usr/sbin/mkfs.vfat"

echo "==> [5/10] Copying GRUB data, console font, terminfo, gconv and modules"
cp -a /usr/lib/grub "$ROOTFS/usr/lib/"
# The 9.3 MiB monolithic grubx64.efi is never used by `grub-install
# --target=x86_64-efi --removable` (it builds the image from kernel.img +
# the .mod files). Dropping it keeps the live OS bootable on 256 MiB RAM.
rm -rf "$ROOTFS/usr/lib/grub/x86_64-efi/monolithic"
# mkdir FIRST: `cp -a /usr/share/grub DST/` with a non-existent DST creates
# DST as a copy of the directory's *contents* instead of DST/grub/ (which
# used to leave /usr/lib/grub/grub-mkconfig_lib dangling).
mkdir -p "$ROOTFS/usr/share/grub"
cp -a /usr/share/grub/. "$ROOTFS/usr/share/grub/"
cp -a /etc/grub.d "$ROOTFS/etc/"
mkdir -p "$ROOTFS/usr/share/consolefonts"
zcat /usr/share/consolefonts/Arabic-VGA16.psf.gz > "$ROOTFS/usr/share/consolefonts/Arabic-VGA16.psf"
mkdir -p "$ROOTFS/usr/share/terminfo"
cp -a /usr/share/terminfo/. "$ROOTFS/usr/share/terminfo/" 2>/dev/null || true
ln -sfn /usr/share/terminfo "$ROOTFS/lib/terminfo" 2>/dev/null || true
# mkfs.fat / grub-install call iconv() and gettext(); without gconv modules
# and the locale directory they print noisy "cannot convert" / "cannot open
# directory" warnings on every boot.
mkdir -p "$ROOTFS/usr/lib/x86_64-linux-gnu/gconv"
cp -a /usr/lib/x86_64-linux-gnu/gconv/. "$ROOTFS/usr/lib/x86_64-linux-gnu/gconv/" 2>/dev/null || true
cp -a /usr/lib/x86_64-linux-gnu/gconv-modules.d "$ROOTFS/usr/lib/x86_64-linux-gnu/" 2>/dev/null || true
cp -a /usr/lib/x86_64-linux-gnu/gconv-modules "$ROOTFS/usr/lib/x86_64-linux-gnu/" 2>/dev/null || true
mkdir -p "$ROOTFS/usr/share/locale"
cp -a /usr/share/locale/aliases "$ROOTFS/usr/share/locale/" 2>/dev/null || true
cp -a /usr/share/locale/locale.alias "$ROOTFS/usr/share/locale/" 2>/dev/null || true
mkdir -p "$ROOTFS/lib/x86_64-linux-gnu"
# Hard-link instead of copy: GNU cpio stores a hardlinked file only once,
# saving ~11 MiB of initramfs and ~11 MiB of RAM during unpacking.
cp -alf "$ROOTFS/usr/lib/x86_64-linux-gnu"/. "$ROOTFS/lib/x86_64-linux-gnu"/ 2>/dev/null || true

MODDST="$ROOTFS/lib/modules/$KVER"
mkdir -p "$MODDST/kernel"
find "$MODROOT" -maxdepth 1 -type f -exec cp -a {} "$MODDST"/ \;
MSRC="$MODROOT/kernel"
for d in block crypto drivers/ata drivers/block drivers/cdrom drivers/hid \
         drivers/input drivers/nvme drivers/scsi drivers/usb drivers/virtio \
         fs lib arch/x86/crypto; do
    if [ -d "$MSRC/$d" ]; then
        mkdir -p "$MODDST/kernel/$(dirname "$d")"
        cp -a "$MSRC/$d" "$MODDST/kernel/$d"
    fi
done
/usr/sbin/depmod -b "$ROOTFS" -a "$KVER" >/dev/null 2>&1 || {
    echo "depmod failed"; exit 1;
}
echo "    modules: $(du -sh "$MODDST" | cut -f1)"

echo "==> [6/10] Installing the installer itself into the rootfs"
mkdir -p "$ROOTFS/usr/local/sbin" "$ROOTFS/sinux/i18n" "$ROOTFS/etc"
install -m 0755 "$SRC/init" "$ROOTFS/init"
install -m 0755 "$SRC/sinux-install" "$ROOTFS/usr/local/sbin/sinux-install"
# regenerate the pre-shaped Farsi console catalog if it is missing
if [ ! -s "$SRC/i18n/fa-console.msg" ]; then
    python3 "$SRC/make_fa_console.py" "$SRC/i18n/fa.msg" "$SRC/i18n/fa-console.msg"
fi
cp "$SRC/i18n/en.msg" "$SRC/i18n/fa.msg" "$SRC/i18n/fa-console.msg" "$ROOTFS/sinux/i18n/"
cp "$KERNEL_ELF" "$ROOTFS/sinux/kernel.elf"
[ -f "$HELLO_ELF" ] && cp "$HELLO_ELF" "$ROOTFS/sinux/hello.elf"
cp "$SRC/README.txt" "$ROOTFS/sinux/README.txt"
cp "$SINUX_SRC/LICENSE" "$ROOTFS/sinux/LICENSE"

cat > "$ROOTFS/etc/profile" <<'EOF'
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PS1='sinux-installer# '
# Do not override TERM: /init selects linux (VGA) or vt100 (serial).
export TERM="${TERM:-linux}"
EOF
cat > "$ROOTFS/etc/sinux-release" <<EOF
NAME="Sinux Installer"
VERSION="$VERSION"
LIVE_OS="Debian 13 minimal Linux (text mode)"
TARGET_OS="Sinux (from-scratch x86_64 kernel, terminal shell)"
EOF
sed -i 's#Arabic-VGA16.psf.gz#Arabic-VGA16.psf#' "$ROOTFS/init"

echo "==> [7/10] Building initrd.img (cpio + XZ)"
# XZ is ~36% smaller than gzip here (35 MB vs 55 MB).  That matters: with
# gzip the 100 MB rootfs could not be unpacked on a 256 MiB machine and the
# kernel panicked with "Initramfs unpacking failed: write error" /
# "No working init found".  CONFIG_RD_XZ=y is present in the Debian kernel.
( cd "$ROOTFS" && find . -print0 | cpio --null -o -H newc -R 0:0 --quiet \
    | xz -9 -T1 --check=crc32 -c ) > "$WORK/initrd.img"
echo "    initrd.img: $(stat -c '%s bytes' "$WORK/initrd.img") (xz)"

echo "==> [8/10] Assembling the ISO stage and running grub-mkrescue"
rm -rf "$STAGE"
mkdir -p "$STAGE/boot/grub" "$STAGE/sinux"
cp "$VMLINUZ" "$STAGE/boot/vmlinuz"
cp "$WORK/initrd.img" "$STAGE/boot/initrd.img"
cp "$KERNEL_ELF" "$STAGE/sinux/kernel.elf"
cp "$SRC/README.txt" "$SINUX_SRC/LICENSE" "$STAGE/sinux/"
cp "$SRC/grub.cfg.iso" "$STAGE/boot/grub/grub.cfg"
ISO="$DIST/$ISO_NAME"
rm -f "$ISO"
grub-mkrescue --modules="multiboot2 serial" -o "$ISO" "$STAGE" -- -volid SINUX_INST > "$WORK/grub-mkrescue.log" 2>&1 || {
    tail -n 40 "$WORK/grub-mkrescue.log"; exit 1;
}
echo "    ISO: $(stat -c '%s bytes' "$ISO")"

echo "==> [9/10] Static checks and rootfs smoke test"
bash -n "$SRC/sinux-install"
sh -n "$SRC/init"
bash -n "$SRC/host-install.sh"
bash -n "$HERE/build.sh"
python3 -m py_compile "$SRC/collect_libs.py" "$SRC/make_fa_console.py"
if command -v shellcheck >/dev/null 2>&1; then
    shellcheck -S warning "$SRC/sinux-install" "$SRC/init" \
        "$SRC/host-install.sh" || echo "    shellcheck: warnings (non-fatal)"
fi
# chroot smoke test: every tool the installer calls must resolve AND run.
# The dangling-symlink scan must run INSIDE the chroot, otherwise absolute
# links such as /sbin/modprobe -> /usr/bin/modprobe resolve against the host.
SMOKE='
set -eu
d=$(find / -xdev -xtype l 2>/dev/null | head -n 20 || true)
[ -z "$d" ] || { echo "DANGLING SYMLINKS:"; echo "$d"; exit 1; }
need() {
  for c in "$@"; do
    command -v "$c" >/dev/null 2>&1 || { echo "MISSING $c"; exit 1; }
    p=$(command -v "$c")
    case "$p" in /*) [ -e "$p" ] || { echo "DANGLING $c -> $p"; exit 1; } ;; esac
  done
}
need sh bash dialog setfont sfdisk partprobe blockdev partx wipefs \
     mkfs.ext2 mkfs.ext4 mkfs.vfat mkfs.fat mke2fs grub-install grub-probe \
     lsblk blkid mount umount modprobe depmod kmod sed grep find sort uniq \
     cut head tail tr wc stat readlink realpath mktemp dirname basename cp mv rm \
     mkdir chmod ln dd sync expr uname ls cat echo printf test true false \
     date sleep tee env pwd install awk od cmp xargs getopt seq id iconv \
     gettext busybox findmnt efibootmgr zcat gzip cpio
/bin/bash -c "echo bash-ok"
/usr/bin/awk "BEGIN{print \"awk-ok\"}"
/usr/bin/dialog --version
/usr/sbin/sfdisk --version
/usr/sbin/grub-install --version
/usr/bin/lsblk --version
/usr/sbin/blkid --version
/usr/bin/modprobe --version
/usr/bin/gettext --version
/usr/sbin/mkfs.vfat --help >/dev/null 2>&1 || true
/usr/bin/setfont --version >/dev/null 2>&1 || true
/bin/bash -n /init
/bin/bash -n /usr/local/sbin/sinux-install
echo "smoke-ok"
'
sudo chroot "$ROOTFS" /bin/bash -c "$SMOKE" || { echo "    rootfs smoke test FAILED"; exit 1; }
echo "    rootfs smoke test: OK"

echo "==> [10/10] Checksums, source bundle, download page and root-level ISO"
# Only OUR installer source is shipped.  The upstream Sinux kernel repository
# is intentionally excluded: it can always be re-downloaded from GitHub, and
# bundling it makes the workspace (and the download) needlessly large.
( cd "$HERE/.." && tar --exclude='.build' --exclude='release' --exclude='Sinux/.git' \
    --exclude='*.iso' --exclude='__pycache__' \
    -czf "$DIST/sinux-installer-source.tar.gz" sinux-installer )
( cd "$DIST" && sha256sum "$ISO_NAME" sinux-installer-source.tar.gz > SHA256SUMS )
ISO_SHA=$(sha256sum "$ISO" | cut -d' ' -f1)
ISO_SIZE=$(stat -c '%s' "$ISO")
sed -e "s/@@VERSION@@/$VERSION/g" \
    -e "s/@@ISO@@/$ISO_NAME/g" \
    -e "s/@@SIZE@@/$ISO_SIZE/g" \
    -e "s/@@SHA256@@/$ISO_SHA/g" \
    "$SRC/index.html.tpl" > "$DIST/index.html"
# A root-level regular file makes the ISO reachable from the workspace
# viewer / download panel; use hard links so the 90 MB payload is stored once.
# With DIST pointed at the workspace root these are the SAME file; skip the
# link instead of failing with "are the same file" under set -e.
[ "$ISO" -ef "$HERE/../$ISO_NAME" ] || ln -f "$ISO" "$HERE/../$ISO_NAME"
echo "    $(cat "$DIST/SHA256SUMS")"
echo "    $ISO_SHA  $ISO_NAME"

if [ "$CLEAN" = 1 ]; then
    echo "==> cleaning heavy build cache"
    rm -rf "$WORK/rootfs" "$WORK/stage" "$KDIR/extracted" "$KDIR"/*.deb "$WORK/initrd.img"
fi
echo "==> DONE: $ISO"
