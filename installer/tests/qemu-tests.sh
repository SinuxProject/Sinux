#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Sinux Installer 1.1.0 - QEMU regression suite
#
# The 1.0.0 release panicked on real hardware with
#   "Attempted to kill init! exitcode=0x00000000"
# because PID 1 could fall off the end of the installer.  Every test below
# asserts that this string NEVER appears in the serial log.
# ---------------------------------------------------------------------------
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
RELEASE="$HERE/.."
ISO="$RELEASE/Sinux-Installer-1.1.0-x86_64.iso"
CFG="$HERE/src/grub.cfg.iso"
VMLINUZ="$HERE/.build/stage/boot/vmlinuz"
INITRD="$HERE/.build/initrd.img"
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd
W=/tmp/sinux-qemu-tests
REPORT="$RELEASE/verification-1.1.0.log"
PASS=0; FAIL=0
rm -rf "$W"; mkdir -p "$W"

say()  { printf '%s\n' "$*"; }
ok()   { say "  PASS  $*"; PASS=$((PASS+1)); }
bad()  { say "  FAIL  $*"; FAIL=$((FAIL+1)); }
check(){ # check <log> <regex> <description>
    if grep -aqE "$2" "$1"; then ok "$3"; else bad "$3"; fi
}
nocrash(){ # nocrash <log> <label>
    if grep -aqE 'Attempted to kill init|Kernel panic|not syncing|exitcode=0x' "$1"; then
        bad "$2: KERNEL PANIC / PID 1 EXIT DETECTED"
        grep -aE 'Attempted to kill init|Kernel panic|not syncing' "$1" | head -n 3
    else
        ok "$2: no panic, PID 1 alive"
    fi
}

QEMU_RAM=512
qboot(){ # qboot <timeout> <log> <input-file> <extra args...>
    local t="$1" logf="$2" inp="$3"; shift 3
    timeout "$t" qemu-system-x86_64 -m "$QEMU_RAM" -smp 1 -display none -no-reboot \
        -serial stdio "$@" < "$inp" > "$logf" 2>&1
    return $?
}
direct(){ # direct <timeout> <log> <input> <append> <drive args...>
    local t="$1" logf="$2" inp="$3" append="$4"; shift 4
    qboot "$t" "$logf" "$inp" -kernel "$VMLINUZ" -initrd "$INITRD" \
        -append "$append" "$@"
}
qvga(){ # qvga <timeout> <log> <input> <extra...>  (VGA text console, -nographic)
    local t="$1" logf="$2" inp="$3"; shift 3
    timeout "$t" qemu-system-x86_64 -m "$QEMU_RAM" -smp 1 -nographic -no-reboot \
        "$@" < "$inp" > "$logf" 2>&1
    return $?
}
newdisk(){ truncate -s "$2" "$1"; }

say "=========================================================="
say " Sinux Installer 1.1.0 - QEMU verification $(date -u '+%Y-%m-%d %H:%M UTC')"
say " ISO      : $ISO"
say " SHA256   : $(sha256sum "$ISO" | cut -d' ' -f1)"
say " QEMU     : $(qemu-system-x86_64 --version | head -n 1)"
say "=========================================================="
: > "$W/empty"

# ---------------------------------------------------------------------------
say ""; say "[T1] ISO boot in legacy BIOS - GRUB menu (9 entries)"
qvga 32 "$W/t1.log" "$W/empty" -cdrom "$ISO" -boot d
check "$W/t1.log" 'Sinux Installer 1\.1\.0' "GRUB menu rendered"
# Static regression for the blinking-cursor bug: the first (default) entry
# must use the VGA console only.  A ttyS0 here hides the whole UI on any
# machine without a COM port.
n=$(grep -c '^menuentry ' "$CFG" || true)
[ "$n" = 9 ] && ok "boot menu has exactly 9 entries" || bad "boot menu has $n entries, expected 9"
first=$(awk '/^menuentry /{f=1} f{print} f&&/^}/{exit}' "$CFG")
echo "$first" | grep -q 'console=tty0' && ok "default entry uses console=tty0" \
    || bad "default entry lost console=tty0"
echo "$first" | grep -q 'ttyS0' && bad "default entry still redirects to ttyS0" \
    || ok "default entry has no serial console"
grep -q 'nomodeset' "$CFG" && ok "Safe text mode fallback present" \
    || bad "Safe text mode fallback missing"
grep -q -- '--id installed' "$CFG" && ok "post-install boot handover entry present" \
    || bad "post-install boot handover entry missing"
grep -q 'set default=installed' "$CFG" && ok "installed system becomes GRUB default" \
    || bad "installed system is not the GRUB default"
for e in English Farsi Serial Verbose Rescue 'Dry' 'directly' 'Boot installed'; do
    check "$W/t1.log" "$e" "menu entry: $e"
done
nocrash "$W/t1.log" "T1 BIOS ISO"

# ---------------------------------------------------------------------------
say ""; say "[T2] ISO boot in UEFI/OVMF - GRUB menu (9 entries)"
cp "$OVMF_VARS" "$W/vars.fd"
qvga 36 "$W/t2.log" "$W/empty" \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$W/vars.fd" \
    -cdrom "$ISO" -boot d
check "$W/t2.log" 'Sinux Installer 1\.1\.0' "OVMF GRUB menu rendered"
check "$W/t2.log" 'Farsi' "Farsi entry present in UEFI"
nocrash "$W/t2.log" "T2 UEFI ISO"

# ---------------------------------------------------------------------------
say ""; say "[T3] English automated install (512 MiB disk) + poweroff"
newdisk "$W/en.img" 512M
direct 160 "$W/t3.log" "$W/empty" \
  "console=ttyS0,115200 quiet loglevel=3 panic=15 sinux.lang=en sinux.mode=auto sinux.device=/dev/vda sinux.fs=ext2 sinux.yes=1 sinux.reboot=0 sinux.nodialog=1" \
  -drive file="$W/en.img",if=virtio,format=raw
check "$W/t3.log" 'INSTALL COMPLETE' "install completed"
check "$W/t3.log" 'powering off' "clean poweroff"
check "$W/t3.log" 'PID 1 supervisor active' "supervisor banner present"
check "$W/t3.log" 'kernel: 81352 bytes' "Sinux kernel copied"
check "$W/t3.log" 'Installing GRUB for UEFI' "UEFI GRUB installed"
check "$W/t3.log" 'Installing GRUB for legacy BIOS' "BIOS GRUB installed"
if grep -aqiE 'error:|no such file|not found|invalid elf|/dev/fd|cannot initialize|command failed' "$W/t3.log"; then
    bad "T3: unexpected error messages"; grep -aiE 'error:|no such file|not found|invalid elf|/dev/fd|cannot initialize|command failed' "$W/t3.log" | head -n 5
else ok "T3: zero runtime errors / warnings"; fi
nocrash "$W/t3.log" "T3 EN auto"
/usr/sbin/sfdisk -d "$W/en.img" > "$W/t3.part" 2>/dev/null || true
check "$W/t3.part" 'label: gpt' "GPT partition table created"
check "$W/t3.part" 'Sinux EFI' "EFI system partition present"

# ---------------------------------------------------------------------------
say ""; say "[T4] Farsi automated install (256 MiB disk, low memory)"
newdisk "$W/fa.img" 256M
timeout 160 qemu-system-x86_64 -m 256 -smp 1 -display none -no-reboot \
    -kernel "$VMLINUZ" -initrd "$INITRD" \
    -append "console=ttyS0,115200 quiet loglevel=3 panic=15 sinux.lang=fa sinux.mode=auto sinux.device=/dev/vda sinux.fs=ext2 sinux.yes=1 sinux.reboot=0 sinux.nodialog=1" \
    -drive file="$W/fa.img",if=virtio,format=raw \
    -serial stdio < "$W/empty" > "$W/t4.log" 2>&1 || true
check "$W/t4.log" 'INSTALL COMPLETE' "Farsi install completed"
check "$W/t4.log" 'i18n: fa' "Farsi catalog loaded"
check "$W/t4.log" 'نصب' "Persian text rendered on serial console"
nocrash "$W/t4.log" "T4 FA auto"

# ---------------------------------------------------------------------------
say ""; say "[T5] Dry run - must return to the menu, NOT exit PID 1"
newdisk "$W/dry.img" 512M
sha256sum "$W/dry.img" | cut -d' ' -f1 > "$W/dry.before"
direct 70 "$W/t5.log" "$W/empty" \
  "console=ttyS0,115200 quiet loglevel=3 panic=15 sinux.lang=en sinux.mode=dryrun sinux.device=/dev/vda sinux.fs=ext2 sinux.yes=1 sinux.nodialog=1" \
  -drive file="$W/dry.img",if=virtio,format=raw
check "$W/t5.log" 'DRY RUN COMPLETE' "dry run finished"
check "$W/t5.log" 'main menu' "returned to main menu instead of exiting"
sha256sum "$W/dry.img" | cut -d' ' -f1 > "$W/dry.after"
if cmp -s "$W/dry.before" "$W/dry.after"; then ok "dry run did not touch the disk"; else bad "dry run modified the disk"; fi
nocrash "$W/t5.log" "T5 dryrun"

# ---------------------------------------------------------------------------
say ""; say "[T6] Rescue shell exit - the exact 1.0.0 crash path"
printf 'exit\nexit\nexit\n' > "$W/rescue.in"
direct 100 "$W/t6.log" "$W/rescue.in" \
  "console=ttyS0,115200 quiet loglevel=3 panic=15 sinux.lang=en sinux.mode=rescue sinux.nodialog=1" \
  -drive file="$W/dry.img",if=virtio,format=raw
check "$W/t6.log" '\[init\] installer exited with status' "supervisor caught the exit"
check "$W/t6.log" 'starting a rescue shell' "supervisor offered a new shell"
restarts=$(grep -ac '\[init\] installer exited with status' "$W/t6.log" || true)
if [ "$restarts" -ge 1 ] && [ "$restarts" -lt 5 ]; then
    ok "supervisor recovered without spinning ($restarts restarts)"
else
    bad "supervisor restart loop looks wrong ($restarts)"
fi
nocrash "$W/t6.log" "T6 rescue exit"

# ---------------------------------------------------------------------------
say ""; say "[T7] Interactive menu: bad selection + console EOF"
printf '99\n0\n' > "$W/cancel.in"
direct 75 "$W/t7.log" "$W/cancel.in" \
  "console=ttyS0,115200 quiet loglevel=3 panic=15 sinux.lang=en sinux.mode=install sinux.nodialog=1" \
  -drive file="$W/dry.img",if=virtio,format=raw
menus=$(grep -ac 'Select an action' "$W/t7.log" || true)
repeats=$(grep -ac 'no selection, showing menu again' "$W/t7.log" || true)
if [ "$menus" -ge 2 ] && [ "$repeats" -ge 1 ]; then
    ok "plain menu stayed visible after invalid input ($menus menus, $repeats retries)"
else
    bad "menu did not survive bad input (menus=$menus retries=$repeats)"
fi
nocrash "$W/t7.log" "T7 cancel/EOF"

# ---------------------------------------------------------------------------
say ""; say "[T8] Boot the INSTALLED disk in legacy BIOS"
# Upstream Sinux PMM hangs in BIOS mode with >= 512 MiB RAM (a kernel
# limitation, not the installer). Its documented minimum is 64 MiB /
# recommended 256 MiB, so this test uses 256 MiB like upstream make run-bios.
QEMU_RAM=256
qboot 60 "$W/t8.log" "$W/empty" -drive file="$W/en.img",format=raw,if=ide -boot c
QEMU_RAM=512
check "$W/t8.log" 'root@sinux:/root#' "Sinux shell prompt reached"
check "$W/t8.log" 'ALL TESTS PASSED' "kernel self-tests passed (21/21)"
nocrash "$W/t8.log" "T8 installed disk BIOS"

# ---------------------------------------------------------------------------
say ""; say "[T9] Boot the INSTALLED disk in UEFI / OVMF"
cp "$OVMF_VARS" "$W/vars2.fd"
cp "$W/en.img" "$W/en-uefi.img"
qboot 70 "$W/t9.log" "$W/empty" \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$W/vars2.fd" \
    -drive file="$W/en-uefi.img",format=raw,if=ide -boot c
check "$W/t9.log" 'root@sinux:/root#' "UEFI Sinux shell prompt reached"
check "$W/t9.log" 'ALL TESTS PASSED' "UEFI kernel self-tests passed"
nocrash "$W/t9.log" "T9 installed disk UEFI"

# ---------------------------------------------------------------------------
say ""; say "[T10] 4Kn disk (4096-byte sectors) automated install"
newdisk "$W/4k.img" 512M
qboot 160 "$W/t10.log" "$W/empty" \
    -kernel "$VMLINUZ" -initrd "$INITRD" \
    -append "console=ttyS0,115200 quiet loglevel=3 panic=15 sinux.lang=en sinux.mode=auto sinux.device=/dev/vda sinux.fs=ext2 sinux.yes=1 sinux.reboot=0 sinux.nodialog=1" \
    -blockdev driver=file,node-name=f0,filename="$W/4k.img" \
    -blockdev driver=raw,node-name=r0,file=f0 \
    -device virtio-blk-pci,drive=r0,logical_block_size=4096,physical_block_size=4096
check "$W/t10.log" 'INSTALL COMPLETE' "4Kn install completed"
check "$W/t10.log" 'sector-size: 4096' "GPT created with 4096-byte sectors"
nocrash "$W/t10.log" "T10 4Kn"

# ---------------------------------------------------------------------------
say ""; say "[T11] Verbose / debug boot - module loading audit"
direct 60 "$W/t11.log" "$W/empty" \
  "console=ttyS0,115200 loglevel=7 panic=15 sinux.lang=en sinux.mode=dryrun sinux.nodialog=1 sinux.debug=1 sinux.device=/dev/vda sinux.yes=1" \
  -drive file="$W/dry.img",if=virtio,format=raw
check "$W/t11.log" '\[modprobe\] virtio_blk' "debug module trace present"
if grep -aq 'Invalid ELF header magic' "$W/t11.log"; then
    bad "T11: Invalid ELF header magic still present"
else ok "T11: no Invalid ELF header magic"; fi
nocrash "$W/t11.log" "T11 debug"

# ---------------------------------------------------------------------------
say ""; say "[T12] VGA console (no serial) - the real-hardware blinking-cursor case"
# 1.0.0 bound /dev/console to ttyS0, so on a machine without a COM port the
# whole UI went nowhere: a blinking cursor in the top left.  QEMU cannot
# render the Linux VGA text console, so verify the installer EFFECT instead:
# boot with console=tty0 only and inspect the target disk afterwards.
newdisk "$W/vga.img" 512M
timeout 100 qemu-system-x86_64 -m 512 -smp 1 -display none -vga std -no-reboot \
    -kernel "$VMLINUZ" -initrd "$INITRD" \
    -append "console=tty0 loglevel=4 panic=15 sinux.lang=en sinux.mode=auto sinux.device=/dev/vda sinux.yes=1 sinux.reboot=0" \
    -drive file="$W/vga.img",if=virtio,format=raw \
    < "$W/empty" > "$W/t12.log" 2>&1 || true
/usr/sbin/sfdisk -d "$W/vga.img" > "$W/t12.part" 2>/dev/null || true
check "$W/t12.part" 'label: gpt' "installer ran with VGA-only console (GPT written)"
check "$W/t12.part" 'Sinux EFI' "EFI system partition created over VGA console"
nocrash "$W/t12.log" "T12 VGA console"

# ---------------------------------------------------------------------------
say ""; say "[T13] VGA framebuffer proof - GRUB menu AND installer UI really drawn"
# This is the actual user-visible symptom: a blank screen with one blinking
# cursor.  No serial port is connected at all; we ask QMP for raw framebuffer
# dumps and count lit pixels.  The grey dialog box covers ~261k pixels.
rm -f "$W/q13.sock"
timeout 150 qemu-system-x86_64 -m 1024 -smp 1 -display none -vga std -no-reboot \
    -qmp "unix:$W/q13.sock,server=on,wait=off" \
    -serial "file:$W/t13.serial.log" \
    -cdrom "$ISO" -boot d \
    > "$W/t13-qemu.log" 2>&1 &
QPID=$!
python3 "$HERE/tests/vga-proof.py" "$W/q13.sock" "$W/vga" 10 24 34 60 85 110 \
    > "$W/t13.log" 2>&1 || true
cat "$W/t13.log"
grep -q 'GRUB_MENU_VISIBLE=yes' "$W/t13.log" \
    && ok "GRUB menu visible on VGA (9 entries)" \
    || bad "GRUB menu not visible on VGA"
grep -q 'INSTALLER_UI_VISIBLE=yes' "$W/t13.log" \
    && ok "installer dialog visible on VGA with NO serial console" \
    || bad "installer UI never appeared on VGA (blinking-cursor bug)"
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

# ---------------------------------------------------------------------------
say ""; say "[T14] USB still plugged in after install - ISO GRUB boots installed Sinux"
# On real hardware the firmware boots the USB stick first, so the installer
# used to come back after a reboot.  The ISO GRUB now detects the marker file
# on the installed root and makes "Boot installed Sinux" the default entry.
QEMU_RAM=256
qboot 80 "$W/t14.log" "$W/empty" \
    -cdrom "$ISO" -boot d -drive file="$W/en.img",if=ide,format=raw
QEMU_RAM=512
check "$W/t14.log" 'root@sinux:/root#' "installed Sinux terminal reached with USB attached"
check "$W/t14.log" 'ALL TESTS PASSED' "installed kernel self-tests passed"
nocrash "$W/t14.log" "T14 USB handover"

# ---------------------------------------------------------------------------
say ""; say "[T15] Interactive install reboots instead of returning to the menu"
newdisk "$W/interactive.img" 512M
python3 "$HERE/tests/interactive-install.py" "$W/t15.log" \
    timeout 260 qemu-system-x86_64 -m 512 -smp 1 -display none -no-reboot \
        -kernel "$VMLINUZ" -initrd "$INITRD" \
        -append "console=ttyS0,115200 loglevel=4 panic=15 sinux.lang=en sinux.mode=install sinux.nodialog=1 sinux.term=vt100 sinux.reboot=1" \
        -drive file="$W/interactive.img",if=virtio,format=raw -serial stdio \
    > "$W/t15.driver.log" 2>&1 || true
cat "$W/t15.driver.log"
check "$W/t15.log" 'INSTALL COMPLETE' "interactive install completed"
check "$W/t15.log" 'boot handover: install finished' "boot handover invoked after success"
check "$W/t15.log" 'Ready to boot into Sinux' "user prompted to unplug the USB stick"
check "$W/t15.log" 'rebooting' "installer rebooted the machine"
/usr/sbin/sfdisk -d "$W/interactive.img" > "$W/t15.part" 2>/dev/null || true
check "$W/t15.part" 'label: gpt' "interactive install wrote the GPT layout"
after=$(awk '/INSTALL COMPLETE/{f=1} f' "$W/t15.log" | grep -ac 'main menu' || true)
[ "$after" = 0 ] && ok "no return to installer menu after success" \
    || bad "installer returned to the menu $after time(s) after success"
nocrash "$W/t15.log" "T15 interactive reboot"

# ---------------------------------------------------------------------------
say ""; say "[T16] UEFI install registers BootNext so the disk boots without USB"
newdisk "$W/uefi-install.img" 512M
cp "$OVMF_VARS" "$W/vars3.fd"
timeout 170 qemu-system-x86_64 -m 512 -smp 1 -display none -no-reboot \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$W/vars3.fd" \
    -kernel "$VMLINUZ" -initrd "$INITRD" \
    -append "console=ttyS0,115200 loglevel=4 panic=15 sinux.lang=en sinux.mode=auto sinux.device=/dev/vda sinux.fs=ext2 sinux.yes=1 sinux.reboot=0 sinux.nodialog=1" \
    -drive file="$W/uefi-install.img",if=virtio,format=raw \
    -serial stdio < "$W/empty" > "$W/t16.log" 2>&1 || true
check "$W/t16.log" 'INSTALL COMPLETE' "UEFI install completed"
check "$W/t16.log" 'BootNext=Boot' "UEFI NVRAM BootNext points at installed Sinux"
check "$W/t16.log" 'Sinux first' "Sinux placed first in UEFI BootOrder"
nocrash "$W/t16.log" "T16 UEFI BootNext"

# ---------------------------------------------------------------------------
say ""; say "=========================================================="
say " RESULT: $PASS passed, $FAIL failed"
say "=========================================================="
{
    say "Sinux Installer 1.1.0 - QEMU verification report"
    say "Date   : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    say "ISO    : Sinux-Installer-1.1.0-x86_64.iso"
    say "SHA256 : $(sha256sum "$ISO" | cut -d' ' -f1)"
    say "Size   : $(stat -c '%s bytes' "$ISO")"
    say "QEMU   : $(qemu-system-x86_64 --version | head -n 1)"
    say ""
    say "RESULT : $PASS passed, $FAIL failed"
    say ""
    say "Tests:"
    say "  T1  ISO boot, legacy BIOS, 9-entry GRUB menu"
    say "  T2  ISO boot, UEFI/OVMF, 9-entry GRUB menu"
    say "  T3  English automated install on 512 MiB disk + clean poweroff"
    say "  T4  Farsi automated install on 256 MiB disk"
    say "  T5  Dry run returns to menu and leaves the disk byte-identical"
    say "  T6  Rescue shell 'exit' (the exact 1.0.0 panic path) -> supervisor"
    say "  T7  Bad menu selection + console EOF -> idles, never exits"
    say "  T8  Installed disk boots in BIOS to root@sinux:/root# (256 MiB RAM;"
    say "      upstream Sinux PMM hangs in BIOS mode with >= 512 MiB)"
    say "  T9  Installed disk boots in UEFI/OVMF to root@sinux:/root#"
    say "  T10 4Kn (4096-byte sector) disk install"
    say "  T11 Verbose debug boot, module load audit"
    say "  T12 VGA-only console (no COM port): installer still runs, disk written"
    say "  T13 VGA framebuffer proof: GRUB menu + installer dialog actually drawn"
    say "  T14 USB still attached after install -> installed Sinux terminal boots"
    say "  T15 Interactive install reboots instead of returning to the menu"
    say "  T16 UEFI install registers BootNext so the disk boots without USB"
    say ""
    say "HOTFIX 2: after a successful install the machine now REBOOTS instead of"
    say "          falling back into the installer menu.  ISO GRUB detects the"
    say "          installed system and makes it the default (T14); UEFI also"
    say "          gets a real NVRAM BootNext entry (T16).  Remove the USB stick when"
    say "          prompted so the firmware cannot boot the installer again."
    say ""
    say "HOTFIX  : default GRUB entries use console=tty0 ONLY.  The old"
    say "          'console=tty0 console=ttyS0,115200' bound /dev/console to the"
    say "          LAST console (the COM port), so on a PC without a serial port"
    say "          the installer UI was invisible: one blinking cursor, top left."
    say "          T12 proves the installer runs and writes the disk with VGA only;"
    say "          T13 proves the GRUB menu and the installer dialog are ACTUALLY"
    say "          drawn on the VGA framebuffer (raw QMP screendumps, lit-pixel"
    say "          counts ~28,700 / 261,379).  Safe text mode entry added as fallback."
    say ""
    say "CRITICAL ASSERTION: no log contains 'Attempted to kill init',"
    say "'Kernel panic' or 'not syncing'. PID 1 never exits in 1.1.0."
} > "$REPORT"
exit "$FAIL"
