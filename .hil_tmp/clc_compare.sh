#!/bin/bash
# Finer HIL marks + CLC sample after naked vs cbs-only vs hil
set -euo pipefail
export PATH=/snap/bin:/usr/bin:/bin:/home/ulipe/.local/aurix/bin:$PATH
source /home/ulipe/.local/aurix/env.sh 2>/dev/null || true

OUT=/home/ulipe/fun/ulmk/.hil_tmp
BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk

# Patch board_services with finer marks via a quick rebuild after editing — 
# For now just SAMPLE clc after each reset type on CURRENT image.

sample() {
	local tag=$1 cfg=$2
	timeout 30 openocd -f "$cfg" -c "
init
halt
resume
sleep 2500
halt
mdw 0x70008280 1
# ASCLIN0 CLC @ 0xF0000600 (TC27x)
mdw 0xF0000600 1
# STM0 CLC @ 0xF0000000
mdw 0xF0000000 1
# SCU OSC / PLLSTAT
mdw 0xF0036010 1
mdw 0xF0036014 1
# WDTCPU0 CON0/CON1 ENDINIT+DR
mdw 0xF0036100 1
mdw 0xF0036104 1
mdw 0xF00360F0 1
mdw 0xF00360F4 1
# CBS_OSTATE / OCNTRL
mdw 0xF000047C 1
mdw 0xF0000480 1
reg PC
shutdown
" >"$OUT/ocd-clc-$tag.log" 2>&1 || true
	{
		echo "===== $tag ====="
		grep -E '0xF000|0xF003|0x7000|PC |Error|halted' "$OUT/ocd-clc-$tag.log" | head -40
	} | tee "$OUT/clc-$tag.txt"
}

sample naked "$OUT/tc275_no_wdt_disarm.cfg"
sleep 1
sample cbs "$OUT/tc275_cbs_only.cfg"
sleep 1
sample hil "$BOARD/openocd/tc275_lite_hil.cfg"

{
	echo "CLC/WDT/CBS compare"
	echo "--- naked ---"; cat "$OUT/clc-naked.txt"
	echo "--- cbs ---"; cat "$OUT/clc-cbs.txt"
	echo "--- hil ---"; cat "$OUT/clc-hil.txt"
} | tee "$OUT/clc-compare.txt"
