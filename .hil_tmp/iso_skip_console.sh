#!/bin/bash
set -uo pipefail
export PATH=/snap/bin:/usr/bin:/bin:/home/ulipe/.local/aurix/bin:$PATH
source /home/ulipe/.local/aurix/env.sh 2>/dev/null || true

OUT=/home/ulipe/fun/ulmk/.hil_tmp
BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk

if ! ss -ltn 2>/dev/null | grep -q ':24817'; then
	nohup /home/ulipe/.local/aurix/bin/tas_server >"$OUT/tas2.log" 2>&1 &
	sleep 2
fi

cd /home/ulipe/fun/ulmk
python3 tools/dev.py build --board ../ulmk_boards/tc275_lite --no-components --component board_blinky
bash "$BOARD/scripts/flash.sh" "$ELF"
pkill -9 -x openocd 2>/dev/null || true
sleep 1

run_sample() {
	local tag=$1 cfg=$2
	pkill -9 -x openocd 2>/dev/null || true
	sleep 0.5
	timeout 45 openocd -f "$cfg" \
		-c "gdb port disabled" \
		-c "init" \
		-c "halt" \
		-c "resume" \
		-c "sleep 4000" \
		-c "halt" \
		-c "mdw 0x70008280 1" \
		-c "reg PC" \
		-c "shutdown" \
		>"$OUT/ocd-isc-$tag.log" 2>&1 || true
	{
		echo "===== $tag ====="
		grep -E '0x70008280|PC \(/32\)|Error:|Examination' "$OUT/ocd-isc-$tag.log" | head -30 || true
		PC=$(grep -E 'PC \(/32\):' "$OUT/ocd-isc-$tag.log" | tail -1 | awk '{print $3}')
		if [[ -n "${PC:-}" ]]; then
			/snap/bin/docker run --rm -v /home/ulipe/fun/build:/build ulipe-microkernel:dev \
				tricore-elf-addr2line -e /build/ulipe-tricore-tc275_lite/ulmk -f -C "$PC" 2>/dev/null || true
		fi
	} | tee "$OUT/isc-$tag.txt"
}

run_sample naked "$OUT/tc275_no_wdt_disarm.cfg"
sleep 1
run_sample hil "$BOARD/openocd/tc275_lite_hil.cfg"

{
	echo "ISOLATE: pinmux+gpio+console skipped; timer still on"
	cat "$OUT/isc-naked.txt"
	echo
	cat "$OUT/isc-hil.txt"
} | tee "$OUT/isc-verdict.txt"
