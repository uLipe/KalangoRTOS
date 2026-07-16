#!/bin/bash
set -euo pipefail
export PATH=/snap/bin:/usr/bin:/bin:/home/ulipe/.local/aurix/bin:$PATH
# shellcheck disable=SC1090
source /home/ulipe/.local/aurix/env.sh 2>/dev/null || true

OUT=/home/ulipe/fun/ulmk/.hil_tmp
NAKED=$OUT/tc275_no_wdt_disarm.cfg
HIL=/home/ulipe/fun/ulmk_boards/tc275_lite/openocd/tc275_lite_hil.cfg
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk

nm "$ELF" | grep -E 'wdt_disable_early|hil_scratch|board_wdt' > "$OUT/nm-wdt.txt"

sample() {
	local tag=$1 cfg=$2
	timeout 30 openocd -f "$cfg" -c "
init
halt
mdw 0xF0036100 1
mdw 0xF0036104 1
mdw 0xF00360F0 1
mdw 0xF00360F4 1
mdw 0xF0036108 1
mdw 0xF00360F8 1
mdw 0x70008280 1
reg PC
shutdown
" >"$OUT/ocd-wdt-$tag.log" 2>&1 || true
	{
		echo "===== $tag ====="
		grep -E '0xF00|0x700|PC \(|Error|halted at|Target' "$OUT/ocd-wdt-$tag.log" | head -40
	} | tee "$OUT/sample-wdt-$tag.txt"
}

sample naked "$NAKED"
sleep 2
sample hil "$HIL"
cat "$OUT/nm-wdt.txt"
cat "$OUT/sample-wdt-naked.txt"
cat "$OUT/sample-wdt-hil.txt"
