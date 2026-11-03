#!/usr/bin/env python3
"""Drive a real interactive (non-auto) install over the serial console.

The regression being checked: after a successful INTERACTIVE install the
installer used to fall straight back into its own main menu instead of
rebooting, so the installed Sinux terminal never appeared.  This driver
answers each plain-text prompt and records the whole session.

Usage: interactive-install.py LOG QEMU_COMMAND [ARGS...]
QEMU must be started with "-serial stdio" and stdin must be this pipe.
Exit code = QEMU's exit code, or 99 on timeout / protocol error.
"""
import os
import subprocess
import sys
import threading
import time

log_path = sys.argv[1]
cmd = sys.argv[2:]
TIMEOUT = 240

# Prompt marker -> what to type.  Handled strictly in order, each only once.
STEPS = [
    (b"Select an action with the arrow keys", b"1\n"),       # Install Sinux
    (b"Choose the disk that will be WIPED", b"1\n"),          # first disk
    (b"Type the exact disk name to confirm", b"vda\n"),       # confirm disk
    (b"Choose the filesystem for the Sinux root", b"1\n"),    # ext2
    (b"Enter a hostname", b"\n"),                             # default hostname
    (b"Start the installation now?", b"y\n"),                 # confirm plan
]

proc = subprocess.Popen(
    cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT, bufsize=0,
)
log = open(log_path, "wb", buffering=0)
state = {"i": 0, "done": False, "err": None}
started = time.time()

def writer():
    try:
        while True:
            chunk = proc.stdout.read(1)
            if not chunk:
                return
            log.write(chunk)
            # Accumulate output so markers spanning reads still match.  Some
            # prompts (ui_input / ui_yesno) do NOT end with a newline -- they
            # print a trailing ": " and then block in read().  So match on
            # every byte, not only on line boundaries.
            state["buf"] = state.get("buf", b"") + chunk
            if state["i"] < len(STEPS):
                marker, keys = STEPS[state["i"]]
                if marker in state["buf"]:
                    print("PROMPT %d -> %r" % (state["i"] + 1, keys), flush=True)
                    proc.stdin.write(keys)
                    proc.stdin.flush()
                    state["i"] += 1
                    state["buf"] = b""
            if b"INSTALL COMPLETE" in state["buf"]:
                state["done"] = True
    except Exception as exc:                       # noqa: BLE001
        state["err"] = repr(exc)
    finally:
        log.flush()

t = threading.Thread(target=writer, daemon=True)
t.start()
rc = None
while time.time() - started < TIMEOUT:
    rc = proc.poll()
    if rc is not None:
        break
    time.sleep(0.3)
else:
    print("TIMEOUT after %ds" % TIMEOUT, flush=True)
    proc.kill()
    rc = 99
try:
    proc.stdin.close()
except OSError:
    pass
t.join(timeout=5)
log.close()
print("INTERACTIVE_RC=%s prompts_answered=%d install_complete=%s" %
      (rc, state["i"], state["done"]), flush=True)
sys.exit(rc if rc is not None else 99)
