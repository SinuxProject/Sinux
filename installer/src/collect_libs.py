#!/usr/bin/env python3
"""Copy executables plus their ELF dependency closure into an initramfs root.

Usage: collect_libs.py ROOT BIN [BIN ...]

Fixes in 1.1.0
  * Full symlink chains are reproduced (/usr/bin/awk -> /etc/alternatives/awk
    -> /usr/bin/mawk used to be a dangling link in the live OS).
  * Every requested program is also flattened at its own path, so a broken or
    missing intermediate link can never make a listed tool unusable.
  * Symlinks are copied as symlinks, real files as real files; the ELF
    interpreter is included even when ldd does not list it.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

if len(sys.argv) < 3:
    sys.exit("usage: collect_libs.py ROOT BIN [BIN ...]")

root = Path(sys.argv[1]).resolve()
initial = [Path(b) for b in sys.argv[2:]]
root.mkdir(parents=True, exist_ok=True)

EXTRA = [
    Path("/usr/bin/awk"),
    Path("/usr/bin/env"),
    Path("/usr/bin/printf"),
    Path("/usr/bin/tee"),
    Path("/usr/bin/sleep"),
    Path("/usr/bin/date"),
    Path("/usr/bin/stat"),
    Path("/usr/bin/head"),
    Path("/usr/bin/tr"),
    Path("/usr/bin/sed"),
    Path("/usr/bin/cat"),
    Path("/usr/bin/id"),
    Path("/usr/bin/mktemp"),
    Path("/usr/bin/clear"),
    Path("/usr/bin/less"),
    Path("/lib64/ld-linux-x86-64.so.2"),
    Path("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2"),
]


def target(p: Path) -> Path:
    return root / str(p).lstrip("/")


def chain(p: Path):
    """Return [p, each symlink hop ..., final real path]."""
    out = []
    cur = Path(p)
    seen = set()
    while cur.is_symlink():
        s = str(cur)
        if s in seen:
            break
        seen.add(s)
        out.append(cur)
        link = os.readlink(cur)
        if not link.startswith("/"):
            link = os.path.join(os.path.dirname(str(cur)), link)
        cur = Path(os.path.normpath(link))
    out.append(cur)
    return out


def copy_as(p: Path, flatten: bool = False) -> None:
    if not p.exists():
        return
    d = target(p)
    d.parent.mkdir(parents=True, exist_ok=True)
    if flatten:
        real = Path(os.path.realpath(str(p)))
        if not real.is_file():
            return
        if d.is_symlink() or d.exists():
            try:
                d.unlink()
            except OSError:
                return
        shutil.copy2(str(real), str(d), follow_symlinks=False)
        return
    if d.exists() or d.is_symlink():
        return
    if p.is_symlink():
        os.symlink(os.readlink(p), str(d))
    elif p.is_file():
        shutil.copy2(str(p), str(d), follow_symlinks=False)


def ldd_libs(p: Path):
    try:
        out = subprocess.run(
            ["ldd", str(p)], capture_output=True, text=True, timeout=20
        ).stdout
    except (OSError, subprocess.SubprocessError):
        return []
    libs = []
    for line in out.splitlines():
        line = line.strip()
        if "=>" in line:
            path = line.split("=>", 1)[1].strip().split()[0]
            if path.startswith("/") and Path(path).is_file():
                libs.append(Path(path))
        elif re.match(r"^/\S+", line) and "ld-linux" in line:
            path = line.split()[0]
            if Path(path).is_file():
                libs.append(Path(path))
    return libs


initial_set = {str(p) for p in initial}
queue = list(initial) + EXTRA
seen = set()
count = 0

while queue:
    p = Path(queue.pop(0))
    key = str(p)
    if key in seen or not p.exists():
        continue
    seen.add(key)

    hops = chain(p)
    for hop in hops[:-1]:
        copy_as(hop)               # reproduce the symlink chain
    real = hops[-1]
    copy_as(real)                  # and the real file at its own location
    if key in initial_set:
        copy_as(p, flatten=True)   # guarantee the requested path works
    count += 1

    if real.is_file():
        for lib in ldd_libs(real):
            if str(lib) not in seen:
                queue.append(lib)

print(f"collect_libs: {count} paths, {len(initial)} requested programs")
