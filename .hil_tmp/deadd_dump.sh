#!/bin/bash
set +x
OUT=/home/ulipe/fun/ulmk/.hil_tmp
exec > "$OUT/deadd.txt" 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG=$OUT/tc275_no_wdt_disarm.cfg
CFG_ATTACH=$OUT/tc275_attach_only.cfg
CFG_HIL=$BOARD/openocd/tc275_lite_hil.cfg
DOCKER=$(command -v docker || echo /snap/bin/docker)
GDB_RAW=$OUT/gdb-deadd.raw
TELNET_RAW=$OUT/telnet-deadd.raw

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.5; }

echo "=== DEADD / trap dump $(date -Is) ==="
echo "ELF=$ELF"
echo "CFG=$CFG"
ls -la "$ELF" "$CFG" "$CFG_ATTACH"
file "$ELF"
echo

kill_ocd

echo "=== PART1: naked reset (no CBS / no WDT disarm), resume 3s, halt ==="
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG" >"$OUT/ocd-deadd.log" 2>&1 &
for i in $(seq 1 30); do ss -ltn | grep -q ':3333' && break; sleep 0.25; done
ss -ltn | grep -E '3333|4444' || echo "WARN: ports not up"
echo "--- ocd-deadd.log ---"
cat "$OUT/ocd-deadd.log"
echo

{
  echo 'reset halt'
  sleep 0.4
  echo resume
} | timeout 6 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
sleep 3

# Telnet dump: regs + CSFRs via mmio + PORT peek
{
  echo halt
  sleep 0.3
  echo 'reg PC'
  echo 'reg PCX'
  echo 'reg PSW'
  echo 'reg a10'
  echo 'reg a11'
  echo 'reg d15'
  echo 'reg fcx'
  echo 'reg lcx'
  # Core0 CSFR MMIO: 0xF8810000 + offset
  # DATR=0xC000 DSTR=0xC004 DEADD=0xC00C
  echo 'mdw 0xF881C000 4'
  echo 'mdw 0xF881C00C 1'
  echo 'mdw 0xF881C004 1'
  # Also try common alt / related
  echo 'mdw 0xF881FE00 4'
  # PORT0 / P10 region peeks (LED / GPIO)
  echo 'mdw 0xF003A000 8'
  echo 'mdw 0xF003B000 8'
  echo 'mdw 0xF0000A00 4'
  # help for mfcr
  echo 'help mfcr'
  echo 'mfcr 0xC00C'
  echo exit
} | timeout 15 nc 127.0.0.1 4444 | tr -d '\000\033' | tee "$TELNET_RAW"
echo
echo '--- telnet done ---'

# GDB: info registers all + CSA walk 8 deep
"$DOCKER" run --rm --network host \
  -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
  ulipe-microkernel:dev \
  timeout 60 tricore-elf-gdb -batch /elf/ulmk \
  -ex 'set pagination off' \
  -ex 'set remotetimeout 25' \
  -ex 'target extended-remote :3333' \
  -ex 'monitor halt' \
  -ex 'printf "halt_PC=%p\n", $pc' \
  -ex 'info symbol $pc' \
  -ex 'printf "PCXI=%p FCX=%p LCX=%p PSW=%p\n", $pcxi, $fcx, $lcx, $psw' \
  -ex 'printf "A10=%p A11=%p D15=%p\n", $a10, $a11, $d15' \
  -ex 'info registers all' \
  -ex 'monitor mdw 0xF881C000 4' \
  -ex 'monitor mdw 0xF881C00C 1' \
  -ex 'monitor mdw 0xF881C004 1' \
  -ex 'printf "mmio_DATR=0x%08x\n", *(unsigned*)0xF881C000' \
  -ex 'printf "mmio_DSTR=0x%08x\n", *(unsigned*)0xF881C004' \
  -ex 'printf "mmio_DEADD=0x%08x\n", *(unsigned*)0xF881C00C' \
  -ex 'set $pcxi=$pcxi' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame0: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame1: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame2: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame3: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame4: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame5: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame6: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame7: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  >"$GDB_RAW" 2>&1 || true

echo '=== GDB raw ==='
cat "$GDB_RAW"
echo

echo '=== addr2line A11 chain (8 frames) + halt PC ==='
mapfile -t A11S < <(grep -E 'A11=0x' "$GDB_RAW" | grep -oE '0x[0-9a-fA-F]{8}' || true)
idx=0
for a in "${A11S[@]}"; do
  sym=$("$DOCKER" run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
    ulipe-microkernel:dev \
    tricore-elf-addr2line -e /elf/ulmk -f -p -a "$a" 2>/dev/null || echo "(addr2line failed)")
  echo "frame$idx A11=$a  =>  $sym"
  idx=$((idx+1))
done
halt_pc=$(grep -oE 'halt_PC=0x[0-9a-fA-F]+' "$GDB_RAW" | head -1 | sed 's/.*=//')
echo "halt_pc=$halt_pc"
if [[ -n "$halt_pc" ]]; then
  "$DOCKER" run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
    ulipe-microkernel:dev \
    tricore-elf-addr2line -e /elf/ulmk -f -p -a "$halt_pc" 2>/dev/null || true
fi

echo
echo '=== DEADD / DATR / DSTR extract ==='
grep -iE 'DEADD|DATR|DSTR|F881C00|mmio_|0x[fF]00' "$TELNET_RAW" "$GDB_RAW" 2>/dev/null | head -80

echo
echo '=== PART2: kill openocd, wait 2s, attach without reset (running target) ==='
# First: leave target RUNNING — resume before kill
{
  echo resume
  sleep 0.2
  echo exit
} | timeout 4 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
kill_ocd
sleep 2
echo "openocd after kill: $(pgrep -af openocd || echo none)"

echo "--- attach with tc275_attach_only.cfg (reset_config none) ---"
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_ATTACH" >"$OUT/ocd-attach-norst.log" 2>&1 &
ATTACH_OK=0
for i in $(seq 1 30); do
  if ss -ltn | grep -q ':3333'; then ATTACH_OK=1; break; fi
  sleep 0.25
done
echo "attach_ports_up=$ATTACH_OK"
cat "$OUT/ocd-attach-norst.log"
echo

if [[ "$ATTACH_OK" = 1 ]]; then
  {
    echo halt
    sleep 0.4
    echo 'reg PC'
    echo 'reg PCX'
    echo 'mdw 0xF881C00C 1'
    echo 'mdw 0x70008280 2'
    echo resume
    sleep 0.2
    echo exit
  } | timeout 12 nc 127.0.0.1 4444 | tr -d '\000\033' | tee "$OUT/telnet-attach-norst.raw"
  echo
  echo "ATTACH_WITHOUT_RESET=SUCCESS"
else
  echo "ATTACH_WITHOUT_RESET=FAIL (ports)"
  echo "--- fallback: hil.cfg halt-only (no reset) ---"
  kill_ocd
  sleep 1
  # strip connect_assert_srst: use attach cfg already; try hil with -c override
  "$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG_HIL" \
    -c 'gdb port 3333; tcl port disabled; telnet_port 4444' \
    >"$OUT/ocd-hil-halt-only.log" 2>&1 &
  # Note: hil.cfg may still assert srst on connect — document result
  for i in $(seq 1 30); do ss -ltn | grep -q ':3333' && break; sleep 0.25; done
  cat "$OUT/ocd-hil-halt-only.log"
  {
    echo halt
    sleep 0.3
    echo 'reg PC'
    echo exit
  } | timeout 10 nc 127.0.0.1 4444 | tr -d '\000\033' | tee "$OUT/telnet-hil-halt.raw" || true
fi

kill_ocd
echo
echo '=== SUMMARY ==='
DEADD=$(grep -oE 'mmio_DEADD=0x[0-9a-fA-F]+' "$GDB_RAW" | head -1)
[[ -z "$DEADD" ]] && DEADD=$(grep -E 'F881C00C' "$TELNET_RAW" | head -3)
echo "DEADD_line=$DEADD"
echo "halt_pc=$halt_pc"
grep -E 'ATTACH_WITHOUT_RESET=' -n "$OUT/deadd.txt" || true
echo DONE_DEADD
