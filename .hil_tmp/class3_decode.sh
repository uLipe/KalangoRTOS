#!/bin/bash
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/class3-decode.out 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG_NAKED=/home/ulipe/fun/ulmk/.hil_tmp/tc275_no_wdt_disarm.cfg
CFG_HIL=$BOARD/openocd/tc275_lite_hil.cfg
OUT=/home/ulipe/fun/ulmk/.hil_tmp
DOCKER=/snap/bin/docker

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.4; }

run_and_gdb() {
  local cfg="$1"
  local tag="$2"
  kill_ocd
  "$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$cfg" >"$OUT/ocd-$tag.log" 2>&1 &
  for i in $(seq 1 20); do ss -ltn | grep -q ':3333' && break; sleep 0.25; done
  { echo 'reset halt'; sleep 0.3; echo resume; } | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
  sleep 4
  "$DOCKER" run --rm --network host \
    -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
    ulipe-microkernel:dev \
    timeout 40 tricore-elf-gdb -batch /elf/ulmk \
    -ex 'set pagination off' \
    -ex 'target extended-remote :3333' \
    -ex 'monitor halt' \
    -ex 'printf "PC=%p\n", $PC' \
    -ex 'info symbol $PC' \
    -ex 'printf "PCX=%p FCX=%p LCX=%p\n", $pcx, $fcx, $lcx' \
    -ex 'set $pcxi=$pcx' \
    -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
    -ex 'printf "csa0=%p\n", $csa' \
    -ex 'x/16wx $csa' \
    -ex 'set $l=*(unsigned*)$csa' \
    -ex 'set $csa1=((($l&0x70000)<<12)|(($l&0xffff)<<6))' \
    -ex 'printf "csa1=%p\n", $csa1' \
    -ex 'x/16wx $csa1' \
    -ex 'set $l2=*(unsigned*)$csa1' \
    -ex 'set $csa2=((($l2&0x70000)<<12)|(($l2&0xffff)<<6))' \
    -ex 'printf "csa2=%p\n", $csa2' \
    -ex 'x/16wx $csa2' \
    >"$OUT/gdb-$tag.out" 2>&1 || true
  echo "=== $tag ==="
  cat "$OUT/gdb-$tag.out"
  kill_ocd
}

echo "=== NAKED (no WDT disarm) ==="
run_and_gdb "$CFG_NAKED" naked3

echo "=== HIL (with WDT disarm) ==="
run_and_gdb "$CFG_HIL" hil3

echo DONE3
