#!/usr/bin/env python3
"""Generate fa-console.msg from fa.msg.

The Linux VGA console has no bidi/shaping engine. We therefore pre-shape
each Persian line with arabic-reshaper + python-bidi and store the visual
order (presentation forms) so setfont(Arabic-VGA16.psf) can display it.

The \n sequences in the catalog are literal two-character escapes because
the installer expands them with printf %b. Shape each line separately.
"""
import sys
from pathlib import Path

try:
    import arabic_reshaper
    from bidi.algorithm import get_display
except ImportError:
    sys.exit("pip install arabic-reshaper python-bidi")

src = Path(sys.argv[1] if len(sys.argv) > 1 else "fa.msg")
dst = Path(sys.argv[2] if len(sys.argv) > 2 else "fa-console.msg")

out = []
for line in src.read_text(encoding="utf-8").splitlines():
    if not line or line.startswith("#") or "=" not in line:
        out.append(line)
        continue
    key, value = line.split("=", 1)
    parts = value.split("\\n")
    shaped = "\\n".join(get_display(arabic_reshaper.reshape(p)) for p in parts)
    out.append(f"{key}={shaped}")

dst.write_text("\n".join(out) + "\n", encoding="utf-8")
print(f"wrote {dst} ({len(out)} lines)")
