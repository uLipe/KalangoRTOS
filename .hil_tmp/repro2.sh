#!/bin/bash
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/repro2.out 2>&1
export PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/bin:/home/ulipe/.local/aurix/bin
cd /home/ulipe/fun/ulmk
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
BUILD=/home/ulipe/fun/build/ulipe-tricore-tc275_lite
ELF=$BUILD/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG_HIL=$BOARD/openocd/tc275_lite_hil.cfg
CFG_NAKED=/home/ulipe/fun/ulmk/.hil_tmp/tc275_no_wdt_disarm.cfg
DOCKER=$(command -v docker || echo /usr/bin/docker)

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.5; }

echo "=== build board_blinky (UP, no smp) ==="
python3 tools/dev.py build --board "$BOARD" --clean --no-components --component board_blinky
ec=$?
echo build_ec=$ec
nm "$ELF" | grep -E 'board_blinky|_ulmk_bmhd|_start|cpu1' | head -30

echo "=== flash ==="
bash "$BOARD/scripts/flash.sh" "$ELF"
echo flash_ec=$?
sleep 2

ocd_daemon() {
  local cfg="$1"
  local log="$2"
  kill_ocd
  "$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$cfg" >"$log" 2>&1 &
  echo ocd_pid=$!
  for i in 1 2 3 4 5 6 7 8 9 10; do
    ss -ltn | grep -q ':4444' && break
    sleep 0.5
  done
}

telnet_cmd() {
  local out="$1"
  shift
  {
    for c in "$@"; do
      printf '%s\n' "$c"
      sleep 0.2
    done
    printf 'exit\n'
  } | timeout 15 nc 127.0.0.1 4444 | tr -d '\000' >"$out"
}

sample() {
  local tag="$1"
  telnet_cmd /home/ulipe/fun/ulmk/.hil_tmp/sample-$tag.txt \
    halt \
    "reg PC" \
    "reg PCX" \
    "reg a11" \
    "mdw 0x70009480 4" \
    "mdw 0xF0036024 1" \
    "mdw 0xF0036100 8" \
    "mdw 0xA0000000 8"
  echo "--- sample $tag ---"
  cat /home/ulipe/fun/ulmk/.hil_tmp/sample-$tag.txt
}

echo "=== A: after flash+start (should be running) — hil daemon, halt sample ==="
ocd_daemon "$CFG_HIL" /home/ulipe/fun/ulmk/.hil_tmp/ocd-a.log
sleep 1
sample after_flash
kill_ocd

echo "=== B: naked reset (no WDT disarm) free-run 4s then sample ==="
ocd_daemon "$CFG_NAKED" /home/ulipe/fun/ulmk/.hil_tmp/ocd-b.log
sleep 1
telnet_cmd /home/ulipe/fun/ulmk/.hil_tmp/cmd-b.txt \
  "reset halt" \
  resume
sleep 4
sample naked_reset
kill_ocd

echo "=== C: hil reset (WITH WDT disarm) free-run 4s then sample ==="
ocd_daemon "$CFG_HIL" /home/ulipe/fun/ulmk/.hil_tmp/ocd-c.log
sleep 1
telnet_cmd /home/ulipe/fun/ulmk/.hil_tmp/cmd-c.txt \
  "reset halt" \
  resume
sleep 4
sample hil_reset
kill_ocd

echo "=== DONE2 ==="
