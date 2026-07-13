#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# tools/hil/hil_ramlog_expect.sh — flash ELF, dump board RAM console via GDB, expect sentinel.
#
# Requires the board to source hil-config.sh first (or export the ULMK_HIL_* vars):
#   ULMK_HIL_FLASH          path to flash.sh
#   ULMK_HIL_OCD            openocd binary
#   ULMK_HIL_OCD_SCRIPTS    openocd script search path (vendor)
#   ULMK_HIL_OCD_BOARD_DIR  board openocd dir (cfg + scripts)
#   ULMK_HIL_OCD_CFG        board openocd cfg filename or path
#   ULMK_HIL_GDB_PORT       gdb remote port (default 3333)
#   ULMK_HIL_GDB_IMAGE      docker image with tricore-elf-gdb (default ulipe-microkernel:dev)
#   ULMK_HIL_BREAK_DONE     optional symbol to break on after boot
#   ULMK_HIL_REQUIRE_TAS    if 1, require tas_server (default 1)
#
# Usage: hil_ramlog_expect.sh <elf> <expect_string> [fail_string]

set -euo pipefail

HIL_DIR="$(cd "$(dirname "$0")" && pwd)"
ELF="${1:?elf}"
EXPECT="${2:?expect string}"
FAIL_STR="${3:-}"

GDB_PORT="${ULMK_HIL_GDB_PORT:-3333}"
GDB_IMAGE="${ULMK_HIL_GDB_IMAGE:-ulipe-microkernel:dev}"
FLASH="${ULMK_HIL_FLASH:?set ULMK_HIL_FLASH (board hil-config.sh)}"
OCD="${ULMK_HIL_OCD:?set ULMK_HIL_OCD}"
OCD_SCRIPTS="${ULMK_HIL_OCD_SCRIPTS:?}"
OCD_BOARD_DIR="${ULMK_HIL_OCD_BOARD_DIR:?}"
OCD_CFG="${ULMK_HIL_OCD_CFG:?}"
REQUIRE_TAS="${ULMK_HIL_REQUIRE_TAS:-1}"
BREAK_DONE="${ULMK_HIL_BREAK_DONE:-}"

if [[ ! -f "$ELF" ]]; then
	echo "ELF not found: ${ELF}" >&2
	exit 1
fi
if [[ "$REQUIRE_TAS" == "1" ]] && ! pgrep -f tas_server >/dev/null; then
	echo "tas_server not running" >&2
	exit 1
fi

echo "ELF:    $(basename "$ELF")"
echo "expect: ${EXPECT}"

"$FLASH" "$ELF" >/dev/null

pkill -9 -f "${OCD}" 2>/dev/null || true
sleep 1
"$OCD" -s "${OCD_SCRIPTS}" -s "${OCD_BOARD_DIR}" \
	-f "${OCD_CFG}" >/tmp/ulmk-ocd-hil.log 2>&1 &
sleep 3

GDB_OUT=/tmp/ulmk-gdb-hil.log
cleanup() { pkill -9 -f "${OCD}" 2>/dev/null || true; }
trap cleanup EXIT

GDB_EX=(
	-ex "set remotetimeout 60"
	-ex "target extended-remote :${GDB_PORT}"
	-ex "monitor reset halt"
	-ex "break ulmk_kern_trap_panic"
)
if [[ -n "$BREAK_DONE" ]]; then
	GDB_EX+=(-ex "break ${BREAK_DONE}")
fi
GDB_EX+=(
	-ex "continue"
	-ex "x/wx &g_ulmk_board_hil_scratch"
	-ex "x/wx &g_ulmk_console_log_len"
	-ex "x/2048cb &g_ulmk_console_log"
	-ex "bt 4"
)

docker run --rm --network host -v "$(dirname "$ELF"):/elf" "$GDB_IMAGE" \
	timeout 90 tricore-elf-gdb -batch "/elf/$(basename "$ELF")" \
	"${GDB_EX[@]}" >"$GDB_OUT" 2>&1 || true

echo "--- gdb ---"
/usr/bin/tail -80 "$GDB_OUT"
echo "--- end gdb ---"

DECODED="$(python3 "${HIL_DIR}/decode_gdb_console.py" "$GDB_OUT")"

echo "--- decoded log ---"
printf '%s\n' "$DECODED"
echo "--- end decoded ---"

if printf '%s' "$DECODED" | grep -qF "$EXPECT"; then
	echo "PASS: ${EXPECT}"
	exit 0
fi

if [[ -n "$FAIL_STR" ]] && printf '%s' "$DECODED" | grep -qF "$FAIL_STR"; then
	echo "FAIL: reported ${FAIL_STR}" >&2
	exit 1
fi

if grep -qE "ulmk_kern_trap_panic" "$GDB_OUT"; then
	echo "FAIL: trap_panic hit" >&2
	exit 1
fi

echo "FAIL: \"${EXPECT}\" not found" >&2
exit 1
