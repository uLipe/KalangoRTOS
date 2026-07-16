#!/bin/bash
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/trap-decode.out 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG_NAKED=/home/ulipe/fun/ulmk/.hil_tmp/tc275_no_wdt_disarm.cfg
DOCKER=$(command -v docker || echo /snap/bin/docker)
OUT=/home/ulipe/fun/ulmk/.hil_tmp

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.4; }

kill_ocd
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_NAKED" >"$OUT/ocd-trap.log" 2>&1 &
for i in $(seq 1 20); do ss -ltn | grep -q ':3333' && break; sleep 0.25; done

# Reset without WDT disarm, run into panic, halt
{
  echo 'reset halt'
  sleep 0.3
  echo resume
  sleep 0.3
} | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
sleep 3

"$DOCKER" run --rm --network host \
  -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
  -v "$OUT:/out" \
  ulipe-microkernel:dev \
  timeout 45 tricore-elf-gdb -batch /elf/ulmk \
  -ex 'set pagination off' \
  -ex 'set remotetimeout 20' \
  -ex 'target extended-remote :3333' \
  -ex 'monitor halt' \
  -ex 'printf "PC=%p\n", $PC' \
  -ex 'info symbol $PC' \
  -ex 'printf "PCX=%p\n", $pcx' \
  -ex 'printf "PSW=%p\n", $psw' \
  -ex 'printf "FCX=%p LCX=%p\n", $fcx, $lcx' \
  -ex 'set $pcxi = $pcx' \
  -ex 'printf "pcxi_raw=0x%08x\n", $pcxi' \
  -ex 'set $csa = ((($pcxi & 0x70000) << 12) | (($pcxi & 0xffff) << 6))' \
  -ex 'printf "csa0=0x%08x\n", $csa' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi1 = *(unsigned int*)$csa' \
  -ex 'set $csa1 = ((($pcxi1 & 0x70000) << 12) | (($pcxi1 & 0xffff) << 6))' \
  -ex 'printf "csa1=0x%08x link=0x%08x\n", $csa1, $pcxi1' \
  -ex 'x/16wx $csa1' \
  -ex 'set $pcxi2 = *(unsigned int*)$csa1' \
  -ex 'set $csa2 = ((($pcxi2 & 0x70000) << 12) | (($pcxi2 & 0xffff) << 6))' \
  -ex 'printf "csa2=0x%08x link=0x%08x\n", $csa2, $pcxi2' \
  -ex 'x/16wx $csa2' \
  -ex 'info symbol *($csa1+12)' \
  -ex 'info symbol *($csa2+12)' \
  >"$OUT/gdb-trap-decode.out" 2>&1 || true

echo '=== gdb-trap-decode.out ==='
cat "$OUT/gdb-trap-decode.out"

kill_ocd
echo DONE_TRAP
