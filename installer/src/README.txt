Sinux Installer 1.1.0
=====================

This ISO is a minimal, terminal-only Linux live system whose ONLY job is to
install Sinux onto a disk. It is not a general-purpose Linux distribution.

Upstream project : https://github.com/SinuxProject/Sinux
Installed OS     : Sinux, an independent x86_64 Multiboot2 kernel written from
                   scratch in C and assembly. It has its own built-in terminal
                   shell. It is NOT Linux and does not run Linux programs.
Live installer OS: Debian 13 kernel + a small busybox/userland initramfs.

What the installer does
-----------------------
1. Detects disks (and excludes this installation media by ISO label, by
   partition labels and by /dev/sr* / loop / read-only devices).
2. Asks for a target disk and requires you to type its name to confirm.
3. Creates a sector-size-independent GPT table bootable by BOTH BIOS and UEFI:
     p1  1 MiB   BIOS Boot Partition
     p2  128 MiB EFI System Partition (FAT32, /boot/efi)
     p3  rest    Sinux root (ext2 by default; ext4 optional)
4. Copies /sinux/kernel.elf to /boot/kernel.elf plus a small /etc skeleton.
5. Installs GRUB twice:
     i386-pc     -> MBR + BIOS Boot Partition
     x86_64-efi  -> /EFI/BOOT/BOOTX64.EFI (removable, no NVRAM write)
6. Writes /boot/grub/grub.cfg which boots the kernel with multiboot2.

Reliability (new in 1.1.0)
--------------------------
* /init is a PID 1 SUPERVISOR. The installer runs as its child; if it ever
  exits (Cancel, dry-run end, rescue shell "exit", a failed command), /init
  starts a rescue shell instead of letting PID 1 die. This prevents the
  "Kernel panic - Attempted to kill init! exitcode=0x00000000" freeze seen
  in 1.0.0 on real hardware.
* panic=15 on the kernel command line: even an unexpected kernel panic
  reboots after 15 s instead of hanging the machine forever.
* If dialog/ncurses is unavailable, the UI automatically falls back to a
  plain-text menu instead of silently failing and exiting.
* Console input EOF (serial/pipe) idles instead of spinning or exiting.
* Full symlink chains are copied into the initramfs, so tools such as awk,
  modprobe and grub-install are never dangling links.
* gconv / locale / terminfo data are present, removing the noisy conversion
  and "cannot open directory" warnings.

Boot menu
---------
  * English (text mode)                      <- default, console=tty0
  * Farsi / Persian (text mode)
  * Safe text mode (VGA only, no dialog)     <- use this if you only see a
                                               blinking cursor in the top
                                               left corner (nomodeset +
                                               sinux.nodialog=1)
  * English (serial console 115200 8N1)      <- serial --unit=0 inside entry
  * Verbose / debug (serial, all kernel messages)
  * Rescue shell (no installer)
  * Dry run (show plan, change nothing)
  * Boot Sinux kernel directly (live, no install)
  * Boot installed Sinux from this disk (system terminal)  <- default AFTER
                                               an install: the USB stick can
                                               stay plugged in

Console handling (1.1.0 hotfix)
-------------------------------
The default entries pass console=tty0 ONLY.  Linux binds /dev/console to the
LAST console= argument, so the old "console=tty0 console=ttyS0,115200" sent
the whole installer UI to a COM port that does not exist on most PCs -- the
symptom was a blank screen with a blinking cursor.  /init now reads
/sys/class/tty/console/active, picks the console the user is actually
looking at and exports it as SINUX_CONSOLE; the installer and its stdio are
attached to that device explicitly.  "quiet" was removed (loglevel=4) so
boot progress and errors are visible.

Kernel command line (edit with 'e' in GRUB)
------------------------------------------
  sinux.lang=en|fa
  sinux.mode=install|auto|dryrun|rescue
  sinux.device=/dev/vda      (auto mode)
  sinux.fs=ext2|ext4
  sinux.hostname=NAME
  sinux.yes=1
  sinux.reboot=0|1
  sinux.debug=1              verbose modprobe + extra logging
  sinux.nodialog=1           force plain text UI
  sinux.term=TERM            override TERM
  sinux.fa_console=auto|shaped|raw

Automated example (for CI / QEMU):
  sinux.mode=auto sinux.device=/dev/vda sinux.yes=1 sinux.reboot=0

Safety
------
The installer NEVER wipes a disk without an explicit typed confirmation
(or sinux.yes=1 in automated mode). Use the "Dry run" menu entry to see the
plan without touching anything. The installation media is excluded from the
disk list via the ISO volume label SINUX_INST.

Known Sinux kernel limitations (upstream)
----------------------------------------
  * Single core, no networking, no USB driver (PS/2 keyboard only)
  * ATA PIO disk driver only; ext2 support is basic (no journal, no fsck)
  * No dynamic linking, no swap
  * Secure Boot must be OFF

Rebuild
-------
  ./build.sh           (needs Debian 13 x86_64 + the packages in README.fa.md)
  ./build.sh --clean   (also drops the heavy build cache)

Post-install boot handover (1.1.0 hotfix 2)
-------------------------------------------
A successful INTERACTIVE install used to fall back into the main menu without
rebooting; a manual "Reboot" then booted the USB stick again because firmware
keeps removable media first.  Now:

  * after INSTALL COMPLETE a dialog asks you to unplug the USB stick and the
    machine reboots automatically (the live OS runs entirely from RAM);
  * ISO GRUB searches for /etc/sinux/install-info.json.  If found, the
    "Boot installed Sinux" entry becomes the default with a 5 s timeout, so
    leaving the stick in still lands at root@sinux:/root#;
  * on UEFI, efibootmgr creates a real NVRAM entry, sets BootNext to it and
    puts Sinux first in BootOrder.  On BIOS this is a harmless no-op.

A line "error: no such device: /etc/sinux/install-info.json" on a SERIAL
console before a first-time boot is just this probe finding nothing; it is
cleared on the VGA console and is not an error condition.
