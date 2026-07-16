#!/bin/bash
# Attach WITHOUT SRST — inspect live red-LED state, then rebuild+flash fixed image.
set +x
exec > /home/ulipe/fun/ulmk/.hil_tmp/button-fix.out 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
cd /home/ulipe/fun/ulmk
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
OUT=/home/ulipe/fun/ulmk/.hil_tmp
ATTACH_CFG=$OUT/tc275_attach_only.cfg

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.4; }

# Ensure tas
if ! pgrep -f tas_server >/dev/null; then
  nohup "$ULMK_TAS_SERVER" >/dev/null 2>&1 &
  sleep 2
fi

cat > "$ATTACH_CFG" <<'EOF'
source [find interface/tas_client.cfg]
set _CHIPNAME tc27x
reset_config none
tas newtap $_CHIPNAME ocds -irlen 8 \
	-expected-id 0x001DA083 -expected-id 0x101DA083 \
	-expected-id 0x201DA083 -expected-id 0x301DA083 \
	-expected-id 0x401DA083 -expected-id 0x501DA083
ocds create tc27x -chain-position $_CHIPNAME.ocds
target create $_CHIPNAME.cpu0 aurix -coreid 0 -ocds tc27x
flash bank pflash0 tc3xx 0x80000000 0x400000 0 0 $_CHIPNAME.cpu0
targets $_CHIPNAME.cpu0
gdb breakpoint_override hard
EOF

echo "=== ATTACH live (no SRST) — current red-LED image ==="
kill_ocd
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$ATTACH_CFG" >"$OUT/ocd-attach-live.log" 2>&1 &
for i in $(seq 1 25); do ss -ltn | grep -q :4444 && break; sleep 0.3; done
{
  echo halt
  echo 'reg PC'
  echo 'reg PCX'
  echo 'mdw 0xF0036024 1'
  echo 'mdw 0x70008280 2'
  echo exit
} | timeout 10 nc 127.0.0.1 4444 | tr -d '\000' | tee "$OUT/live-red.txt"
PC=$(sed -n 's/.*PC (\/32): //p' "$OUT/live-red.txt" | head -1)
echo LIVE_PC=$PC
if [ -n "$PC" ] && [ -f "$ELF" ]; then
  docker run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf ulipe-microkernel:dev \
    tricore-elf-addr2line -e /elf/ulmk -f -a "$PC" 2>/dev/null || true
fi
kill_ocd

echo "=== rebuild blinky (CBS poke removed) ==="
python3 tools/dev.py build --board "$BOARD" --no-components --component board_blinky
echo BUILD:$?

echo "=== flash ==="
bash "$BOARD/scripts/flash.sh" "$ELF"
echo FLASH:$?
sleep 2

echo "=== simulate PORST-like: naked reset (no CBS reset-end) ==="
kill_ocd
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$OUT/tc275_no_wdt_disarm.cfg" >"$OUT/ocd-naked2.log" 2>&1 &
for i in $(seq 1 20); do ss -ltn | grep -q :4444 && break; sleep 0.25; done
{ echo 'reset halt'; sleep 0.3; echo resume; } | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
sleep 5
{
  echo halt
  echo 'reg PC'
  echo 'mdw 0x70008280 2'
  echo exit
} | timeout 8 nc 127.0.0.1 4444 | tr -d '\000' | tee "$OUT/naked-after-fix.txt"
PC2=$(sed -n 's/.*PC (\/32): //p' "$OUT/naked-after-fix.txt" | head -1)
echo NAKED_PC=$PC2
docker run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf ulipe-microkernel:dev \
  tricore-elf-addr2line -e /elf/ulmk -f -a "$PC2" 2>/dev/null || true
kill_ocd

echo "=== hil control ==="
kill_ocd
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$BOARD/openocd/tc275_lite_hil.cfg" >"$OUT/ocd-hil2.log" 2>&1 &
for i in $(seq 1 20); do ss -ltn | grep -q :4444 && break; sleep 0.25; done
{ echo 'reset halt'; sleep 0.3; echo resume; } | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
sleep 5
{
  echo halt
  echo 'reg PC'
  echo 'mdw 0x70008280 2'
  echo exit
} | timeout 8 nc 127.0.0.1 4444 | tr -d '\000' | tee "$OUT/hil-after-fix.txt"
PC3=$(sed -n 's/.*PC (\/32): //p' "$OUT/hil-after-fix.txt" | head -1)
echo HIL_PC=$PC3
docker run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf ulipe-microkernel:dev \
  tricore-elf-addr2line -e /elf/ulmk -f -a "$PC3" 2>/dev/null || true
kill_ocd

echo DONE_BUTTON_FIX
