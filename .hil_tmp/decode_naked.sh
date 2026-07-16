#!/bin/bash
# Decode naked trap: PC/symbols + DIEAR + WDT + scratch
set -euo pipefail
export PATH=/snap/bin:/usr/bin:/bin:/home/ulipe/.local/aurix/bin:$PATH
source /home/ulipe/.local/aurix/env.sh 2>/dev/null || true

OUT=/home/ulipe/fun/ulmk/.hil_tmp
NAKED=$OUT/tc275_no_wdt_disarm.cfg
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
GDB=tricore-elf-gdb
ADDR2LINE=tricore-elf-addr2line

# Ensure TAS up via a quick hil reset+run first so image is loaded? assume already flashed
timeout 25 openocd -f "$NAKED" -c "
init
halt
resume
sleep 3000
halt
mdw 0x70008280 1
mdw 0xF0036100 1
mdw 0xF0036104 1
mdw 0xF00360F0 1
mdw 0xF00360F4 1
mdw 0xF0036108 1
mdw 0xF00360F8 1
mdw 0xFE00 1
mdw 0xFE38 1
mdw 0xFE3C 1
# Class4 data error SFRs (CPU0)
mdw 0xF7E1A010 1
mdw 0xF7E1A014 1
mdw 0xF7E1A018 1
reg PC
reg D15
shutdown
" >"$OUT/ocd-decode-naked.log" 2>&1 || true

# Extract PCXI and walk via gdb
timeout 20 openocd -f "$OUT/tc275_attach_only.cfg" -c "init; halt" >"$OUT/ocd-att-dec.log" 2>&1 &
OCDPID=$!
sleep 2

cat >"$OUT/decode.gdb" <<'EOF'
set pagination off
target remote :3333
printf "PC=0x%lx\n", $pc
printf "D15=%lu\n", $d15
printf "PCXI=0x%lx FCX=0x%lx LCX=0x%lx\n", $pcxi, $fcx, $lcx
# DIEAR/DIETR if visible as CSFR — try mmio
set $diear = *(unsigned int*)0xF7E1A014
set $dietr = *(unsigned int*)0xF7E1A018
set $datr = *(unsigned int*)0xF7E1A010
printf "DATR=0x%x DIEAR=0x%x DIETR=0x%x\n", $datr, $diear, $dietr
# Walk up to 6 CSA frames from PCXI
set $link = $pcxi
set $i = 0
while ($i < 6 && $link != 0)
  set $addr = ((($link & 0x70000) << 12) | (($link & 0xffff) << 6))
  printf "csa[%d] link=0x%x addr=0x%x\n", $i, $link, $addr
  x/16xw $addr
  set $link = *(unsigned int*)$addr
  set $i = $i + 1
end
detach
quit
EOF

"$GDB" -nx -batch -x "$OUT/decode.gdb" "$ELF" >"$OUT/gdb-decode.out" 2>&1 || true
kill $OCDPID 2>/dev/null || true
wait $OCDPID 2>/dev/null || true

# Symbolize interesting addrs from gdb dump
{
  echo "=== ocd regs ==="
  grep -E '0xF00|0x700|0xF7E|PC |D15|Error|halted' "$OUT/ocd-decode-naked.log" | head -50
  echo "=== gdb ==="
  cat "$OUT/gdb-decode.out"
  echo "=== addr2line hot PCs ==="
  for a in 0xa0006d58 0xa000019c 0xa00032b2 0xa0005ee0 0xa0005eeb 0xa0003204 0xa000316c; do
    echo -n "$a: "
    "$ADDR2LINE" -e "$ELF" -f -C "$a" 2>/dev/null | tr '\n' ' '
    echo
  done
  nm "$ELF" | grep -E 'wdt_disable_early|pinmux_init|hil_mark'
} | tee "$OUT/decode-summary.txt"
