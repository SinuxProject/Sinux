#!/bin/bash
# Sinux headless QEMU smoke test.
#
# Boots the ISO, captures serial output, and asserts the boot markers.
# Exit 0 = kernel booted cleanly. Anything else = failure.
#
# NOTE: QEMU is invoked directly (same flags as `run-serial` in
# scripts/qemu.mk) instead of via `make run-serial`, so that `timeout`
# signals QEMU itself. Through make, QEMU would be orphaned and the
# timeout would never stop it.
set -u

ISO="build/iso/sinux.iso"
DISK="sinux.img"
LOG="serial.log"
TIMEOUT_S="${SMOKE_TIMEOUT:-120}"

[ -f "$ISO" ]  || { echo "[FAIL] $ISO not found (run 'make iso' first)"; exit 1; }
[ -f "$DISK" ] || { echo "[FAIL] $DISK not found (run 'make iso' first)"; exit 1; }

echo "[..] booting $ISO in QEMU (headless, ${TIMEOUT_S}s timeout)..."
timeout --signal=TERM --kill-after=10s "$TIMEOUT_S" \
  qemu-system-x86_64 \
    -cdrom "$ISO" -m 256M -device bochs-display \
    -drive "file=${DISK},format=raw,if=ide,index=0,media=disk" \
    -nographic -serial mon:stdio -no-reboot \
    > "$LOG" 2>&1 || code=$?
code=${code:-0}
echo "[..] qemu exit code: $code (124/137 = expected timeout; kernel runs forever)"

fail=0
check_present() {
  if grep -a -q "$1" "$LOG"; then echo "[OK] marker found: $1";
  else echo "[FAIL] marker missing: $1"; fail=1; fi
}
check_absent() {
  if grep -a -q "$1" "$LOG"; then echo "[FAIL] forbidden marker present: $1"; fail=1;
  else echo "[OK] absent as expected: $1"; fi
}

# Boot banner (tty_puts -> serial), memory line, and the expected end
# state: no init ELF exists yet, so the kernel must reach its shell.
check_present "Made By SUN"
check_present "RAM:"
check_present "falling back to kernel shell"
check_absent  "panic"

if [ "$fail" -ne 0 ]; then
  echo "=== $LOG (tail) ==="
  tail -40 "$LOG"
  exit 1
fi
echo "[OK] smoke test passed"
