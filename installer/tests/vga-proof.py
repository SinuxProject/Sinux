#!/usr/bin/env python3
"""Prove the installer UI is really drawn on the VGA framebuffer.

Regression for the 1.1.0 console hotfix.  The old default cmdline was
    console=tty0 console=ttyS0,115200
and Linux binds /dev/console to the LAST console= argument, i.e. the COM port.
On a normal PC without a serial port the result was a blank screen with a
blinking cursor in the top-left corner: the installer was running but nobody
could see it.

QEMU cannot show the Linux VGA text console on stdout, so we ask QMP for raw
PPM screendumps and count lit pixels.  Measured on the fixed ISO:

    GRUB 8-entry menu        ~27,000 lit pixels
    "Loading ... please wait" ~2,800
    installer dialog box     ~261,000 lit pixels  <-- the UI is visible

Usage: vga-proof.py <qmp-socket> <out-dir> <seconds> [<seconds> ...]
Prints the lit-pixel count for each capture and exits 0 only when BOTH the
GRUB menu and the installer dialog were actually rendered.
"""
import json
import os
import socket
import sys
import time

MENU_MIN = 10000     # GRUB menu with 8 rows of text
UI_MIN = 100000      # grey dialog box covering ~35% of 720x400


def qmp_connect(path):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    for _ in range(150):
        try:
            s.connect(path)
            break
        except OSError:
            time.sleep(0.2)
    else:
        raise SystemExit("cannot connect to QMP socket " + path)
    f = s.makefile("rwb")

    def cmd(obj):
        f.write((json.dumps(obj) + "\n").encode())
        f.flush()
        while True:
            line = f.readline()
            if not line:
                return {}
            try:
                d = json.loads(line)
            except ValueError:
                continue
            if "return" in d or "error" in d:
                return d
    cmd({"execute": "qmp_capabilities"})
    return s, cmd


def lit_pixels(path):
    """Count pixels that are clearly not black in a QEMU P6 PPM screendump."""
    with open(path, "rb") as fh:
        data = fh.read()
    # QEMU writes the simple form: P6\n<w> <h>\n<maxval>\n<binary RGB>
    fields = data.split(b"\n", 3)
    if len(fields) != 4 or fields[0] != b"P6":
        raise ValueError("unexpected PPM header in " + path)
    w, h = (int(x) for x in fields[1].split())
    raw = fields[3]
    n = min(w * h, len(raw) // 3)
    lit = 0
    for i in range(n):
        p = 3 * i
        if raw[p] > 30 or raw[p + 1] > 30 or raw[p + 2] > 30:
            lit += 1
    return lit


def main():
    sock_path, out_dir = sys.argv[1], sys.argv[2]
    times = [float(x) for x in sys.argv[3:]]
    os.makedirs(out_dir, exist_ok=True)
    sock, cmd = qmp_connect(sock_path)
    prev = 0.0
    menu_seen = False
    ui_seen = False
    for t in times:
        time.sleep(max(0.0, t - prev))
        prev = t
        ppm = os.path.join(out_dir, "vga-t%03d.ppm" % int(t))
        r = cmd({"execute": "screendump",
                 "arguments": {"filename": ppm, "format": "ppm"}})
        if "error" in r:
            print("screendump error at %.0fs: %s" % (t, r["error"]))
            continue
        n = lit_pixels(ppm)
        print("VGA t=%3.0fs lit_pixels=%d" % (t, n), flush=True)
        if n >= MENU_MIN:
            menu_seen = True
        if n >= UI_MIN:
            ui_seen = True
    sock.close()
    print("GRUB_MENU_VISIBLE=%s INSTALLER_UI_VISIBLE=%s" %
          ("yes" if menu_seen else "no", "yes" if ui_seen else "no"))
    return 0 if (menu_seen and ui_seen) else 1


if __name__ == "__main__":
    sys.exit(main())
