#!/bin/bash
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/verify2.out 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
cd /home/ulipe/fun/ulmk
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG_NAKED=/home/ulipe/fun/ulmk/.hil_tmp/tc275_no_wdt_disarm.cfg
OUT=/home/ulipe/fun/ulmk/.hil_tmp

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.4; }

echo "=== rebuild blinky (I2C fine-init deferred) ==="
python3 tools/dev.py build --board "$BOARD" --no-components --component board_blinky
echo build_ec=$?

bash "$BOARD/scripts/flash.sh" "$ELF"
sleep 1

kill_ocd
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_NAKED" >"$OUT/ocd-v2.log" 2>&1 &
for i in $(seq 1 20); do ss -ltn | grep -q ':4444' && break; sleep 0.25; done
{ echo 'reset halt'; sleep 0.3; echo resume; } | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
sleep 5
{
  echo halt
  echo 'reg PC'
  echo 'mdw 0x70009480 4'
  echo exit
} | timeout 8 nc 127.0.0.1 4444 | tr -d '\000' | tee "$OUT/verify2-sample.txt"
PC=$(sed -n 's/.*PC (\/32): //p' "$OUT/verify2-sample.txt" | head -1)
echo PC=$PC
docker run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
  ulipe-microkernel:dev tricore-elf-addr2line -e /elf/ulmk -f -a "$PC" 2>/dev/null || true
kill_ocd
echo DONE2
