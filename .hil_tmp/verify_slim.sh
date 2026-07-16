#!/bin/bash
# Build blinky, flash, sample NAKED first (no prior CBS), then CBS, then HIL.
set -uo pipefail
export PATH=/snap/bin:/usr/bin:/bin:/home/ulipe/.local/aurix/bin:$PATH
source /home/ulipe/.local/aurix/env.sh 2>/dev/null || true

OUT=/home/ulipe/fun/ulmk/.hil_tmp
BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
ULMK=/home/ulipe/fun/ulmk

cd "$ULMK"
python3 tools/dev.py build --board ../ulmk_boards/tc275_lite --no-components --component board_blinky
bash "$BOARD/scripts/flash.sh" "$ELF"

# Kill any leftover openocd
pkill -9 -x openocd 2>/dev/null || true
sleep 1

sample() {
	local tag=$1 cfg=$2 attempt
	for attempt in 1 2 3; do
		pkill -9 -x openocd 2>/dev/null || true
		sleep 0.5
		timeout 40 openocd -f "$cfg" -c "
gdb port disabled
init
halt
resume
sleep 4000
halt
mdw 0x70008280 1
reg PC
shutdown
" >"$OUT/ocd-slim-$tag.log" 2>&1 || true
		if grep -q '0x70008280:' "$OUT/ocd-slim-$tag.log"; then
			break
		fi
		echo "retry $tag attempt=$attempt" >>"$OUT/ocd-slim-$tag.log"
		sleep 1
	done
	{
		echo "===== $tag ====="
		grep -E '0x70008280|PC \(/32\)|Error:' "$OUT/ocd-slim-$tag.log" | head -20 || true
		PC=$(grep -E 'PC \(/32\):' "$OUT/ocd-slim-$tag.log" | tail -1 | awk '{print $3}')
		if [[ -n "${PC:-}" ]]; then
			/snap/bin/docker run --rm -v /home/ulipe/fun/build:/build ulipe-microkernel:dev \
				tricore-elf-addr2line -e /build/ulipe-tricore-tc275_lite/ulmk -f -C "$PC" 2>/dev/null | tr '\n' ' ' || true
			echo
		fi
	} >"$OUT/slim-$tag.txt"
	cat "$OUT/slim-$tag.txt"
}

sample naked "$OUT/tc275_no_wdt_disarm.cfg"
sleep 1
sample cbs "$OUT/tc275_cbs_only.cfg"
sleep 1
sample hil "$BOARD/openocd/tc275_lite_hil.cfg"

{
	echo "VERDICT slim board_init (ASCLIN+STM only)"
	cat "$OUT/slim-naked.txt"
	echo
	cat "$OUT/slim-cbs.txt"
	echo
	cat "$OUT/slim-hil.txt"
	echo
	if grep -q 'ulmk_arch_cpu_idle' "$OUT/slim-naked.txt"; then
		echo "NAKED: PASS"
	elif grep -q '0x70008280: 00000003' "$OUT/slim-naked.txt"; then
		echo "NAKED: PASS (scratch=3)"
	else
		echo "NAKED: FAIL"
	fi
} | tee "$OUT/slim-verdict.txt"
