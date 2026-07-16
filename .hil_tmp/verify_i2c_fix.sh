#!/bin/bash
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/verify-fix.out 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
cd /home/ulipe/fun/ulmk
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG_NAKED=/home/ulipe/fun/ulmk/.hil_tmp/tc275_no_wdt_disarm.cfg
CFG_HIL=$BOARD/openocd/tc275_lite_hil.cfg
OUT=/home/ulipe/fun/ulmk/.hil_tmp

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.4; }

echo "=== build board_blinky UP ==="
python3 tools/dev.py build --board "$BOARD" --clean --no-components --component board_blinky
echo build_ec=$?
nm "$ELF" | grep -E 'board_blinky|smp_can|trap_panic' | head -20

echo "=== flash ==="
bash "$BOARD/scripts/flash.sh" "$ELF"
echo flash_ec=$?
sleep 2

sample_after_naked() {
  kill_ocd
  "$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_NAKED" >"$OUT/ocd-verify.log" 2>&1 &
  for i in $(seq 1 20); do ss -ltn | grep -q ':4444' && break; sleep 0.25; done
  {
    echo 'reset halt'
    sleep 0.3
    echo resume
  } | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
  sleep 4
  {
    echo halt
    echo 'reg PC'
    echo 'mdw 0x70009480 4'
    echo exit
  } | timeout 8 nc 127.0.0.1 4444 | tr -d '\000' >"$OUT/verify-naked-sample.txt"
  echo '--- naked sample ---'
  cat "$OUT/verify-naked-sample.txt"
  kill_ocd
}

sample_after_naked

# Resolve PC
PC=$(sed -n 's/.*PC (\/32): //p' "$OUT/verify-naked-sample.txt" | head -1)
echo "PC_raw=$PC"
/snap/bin/docker run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
  ulipe-microkernel:dev tricore-elf-addr2line -e /elf/ulmk -f -a "$PC" 2>/dev/null || true

echo DONE_VERIFY
