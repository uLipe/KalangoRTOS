#!/bin/bash
# Reproduce button-like reset: NO OpenOCD WDT disarm, then free-run and sample PC.
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/repro-reset.out 2>&1
export PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG_HIL=$BOARD/openocd/tc275_lite_hil.cfg
CFG_NAKED=/home/ulipe/fun/ulmk/.hil_tmp/tc275_no_wdt_disarm.cfg
DOCKER=$(command -v docker || echo /usr/bin/docker)

echo "=== ensure blinky symbols ==="
nm "$ELF" | grep -E 'board_blinky|bmhd|_start' | head -20

echo "=== tas ==="
if ! pgrep -f tas_server >/dev/null; then
  nohup "$ULMK_TAS_SERVER" >/home/ulipe/fun/ulmk/.hil_tmp/tas.log 2>&1 &
  sleep 2
fi
pgrep -af tas_server | head -2

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.3; }

echo "=== flash blinky (normal path with WDT disarm) ==="
# Rebuild/flash only if needed — try flash start
bash "$BOARD/scripts/flash.sh" "$ELF" || { echo FLASH_FAIL; exit 1; }

echo "=== wait 2s for blinky to run ==="
sleep 2

echo "=== NOW: naked reset (no WDT disarm) + resume 3s + halt ==="
kill_ocd
timeout 20 "$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_NAKED" \
  -c "gdb port disabled; init; reset halt; resume; sleep 3000; halt; reg PC; reg PCX; mdw 0x70009480 4; mdw 0xF0036024 1; shutdown" \
  > /home/ulipe/fun/ulmk/.hil_tmp/repro-naked.log 2>&1 || true
echo "--- naked log ---"
cat /home/ulipe/fun/ulmk/.hil_tmp/repro-naked.log | tail -40

echo "=== control: hil.cfg reset (WITH WDT disarm) + resume 3s + halt ==="
kill_ocd
timeout 20 "$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_HIL" \
  -c "gdb port disabled; init; reset halt; resume; sleep 3000; halt; reg PC; reg PCX; mdw 0x70009480 4; mdw 0xF0036024 1; shutdown" \
  > /home/ulipe/fun/ulmk/.hil_tmp/repro-hil.log 2>&1 || true
echo "--- hil log ---"
cat /home/ulipe/fun/ulmk/.hil_tmp/repro-hil.log | tail -40

echo "=== DONE ==="
