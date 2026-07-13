#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# tools/hil/hil_serial_expect.sh — flash ELF, capture serial until pattern/timeout.
#
# Requires board hil-config (or exports):
#   ULMK_HIL_FLASH, ULMK_HIL_SERIAL, ULMK_HIL_SERIAL_BAUD (default 115200)
# Optional:
#   ULMK_HIL_OCD, ULMK_HIL_OCD_SCRIPTS, ULMK_HIL_OCD_BOARD_DIR, ULMK_HIL_OCD_CFG
#   — if set, re-resume target after opening the serial port
#
# Usage: hil_serial_expect.sh <elf> <regex_pattern> [timeout_s]

set -euo pipefail

ELF="${1:?elf}"
PATTERN="${2:?regex pattern}"
TIMEOUT_S="${3:-30}"

FLASH="${ULMK_HIL_FLASH:?set ULMK_HIL_FLASH}"
SERIAL="${ULMK_HIL_SERIAL:-/dev/ttyUSB0}"
BAUD="${ULMK_HIL_SERIAL_BAUD:-115200}"

pkill -9 -x openocd 2>/dev/null || true
fuser -k "$SERIAL" 2>/dev/null || true
sleep 0.3

"$FLASH" "$ELF"

OUT="$(mktemp)"
python3 - "$OUT" "$PATTERN" "$TIMEOUT_S" "$SERIAL" "$BAUD" <<'PY' &
import serial, time, sys, re

out, pat, to, port, baud = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4], int(sys.argv[5])
rx = re.compile(pat.encode() if isinstance(pat, str) else pat)
s = serial.Serial(port, baud, timeout=0.2)
time.sleep(0.05)
s.reset_input_buffer()
buf = b''
t0 = time.time()
while time.time() - t0 < to:
	d = s.read(4096)
	if d:
		buf += d
	if rx.search(buf):
		time.sleep(0.5)
		buf += s.read(8192)
		break
open(out, 'wb').write(buf)
s.close()
print(f'captured {len(buf)} bytes -> {out}', file=sys.stderr)
PY
CAP=$!
sleep 0.2

if [[ -n "${ULMK_HIL_OCD:-}" && -n "${ULMK_HIL_OCD_CFG:-}" ]]; then
	pkill -9 -x openocd 2>/dev/null || true
	"${ULMK_HIL_OCD}" -s "${ULMK_HIL_OCD_SCRIPTS}" -s "${ULMK_HIL_OCD_BOARD_DIR}" \
		-f "${ULMK_HIL_OCD_CFG}" \
		-c "gdb port disabled; init; reset halt; resume; shutdown" \
		>/dev/null 2>&1 || true
fi

wait "$CAP"
echo "===== uart ====="
cat "$OUT"
rm -f "$OUT"
