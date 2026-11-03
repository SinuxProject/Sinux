#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# host-install.sh - safely write the Sinux Installer ISO to a USB stick
# from an ordinary Linux machine. Run as root (or with sudo).
# ---------------------------------------------------------------------------
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ISO="${1:-$HERE/../../Sinux-Installer-1.1.0-x86_64.iso}"

[ -f "$ISO" ] || { echo "ISO not found: $ISO" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo $0 /path/to.iso)" >&2; exit 1; }

echo "ISO: $ISO ($(du -h "$ISO" | cut -f1))"
echo
echo "Available disks:"
lsblk -d -n -o NAME,SIZE,TYPE,MODEL | awk '$3=="disk" {printf "  /dev/%-12s %-8s %s\n",$1,$2,substr($0,index($0,$4))}'
echo
printf 'Target device (e.g. /dev/sdb): '; read -r DEV
[ -b "$DEV" ] || { echo "Not a block device: $DEV" >&2; exit 1; }
case "$DEV" in /dev/sd*|/dev/nvme*n1|/dev/mmcblk*|/dev/vd*) ;; *) echo "Refusing suspicious device"; exit 1;; esac

# Never allow overwriting the disk that holds the running system.
ROOT_SRC="$(findmnt -no SOURCE / 2>/dev/null || true)"
ROOT_PARENT="$(lsblk -no PKNAME "$ROOT_SRC" 2>/dev/null | head -n 1 | tr -d ' ' || true)"
if [ -n "$ROOT_PARENT" ] && [ "/dev/$ROOT_PARENT" = "$DEV" ]; then
    echo "Refusing: $DEV contains the running root filesystem ($ROOT_SRC)." >&2
    exit 1
fi
case "$ROOT_SRC" in "$DEV"*) echo "Refusing: $DEV contains the running root filesystem." >&2; exit 1;; esac

echo
echo "!!! ALL DATA ON $DEV WILL BE DESTROYED !!!"
printf 'Type the device name again to confirm: '; read -r CONF
[ "$CONF" = "$DEV" ] || { echo "Mismatch, aborting."; exit 1; }

echo "Unmounting any partitions on $DEV ..."
for p in "$DEV"*; do [ -b "$p" ] && umount "$p" 2>/dev/null || true; done

echo "Writing with dd ..."
dd if="$ISO" of="$DEV" bs=4M status=progress conv=fsync
sync
echo "Done. Safely remove the USB stick, disable Secure Boot, and boot from it."
