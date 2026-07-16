#!/bin/bash
set +x
OUT=/home/ulipe/fun/ulmk/.hil_tmp
exec > "$OUT/class4-again.txt" 2>&1
export PATH=/snap/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
source /home/ulipe/.local/aurix/env.sh

BOARD=/home/ulipe/fun/ulmk_boards/tc275_lite
ELF=/home/ulipe/fun/build/ulipe-tricore-tc275_lite/ulmk
OCD=$ULMK_AURIX_PREFIX/bin/openocd
SCRIPTS=$ULMK_AURIX_PREFIX/share/openocd/scripts
CFG=$OUT/tc275_no_wdt_disarm.cfg
DOCKER=$(command -v docker || echo /snap/bin/docker)
GDB_RAW=$OUT/gdb-class4-again.raw

kill_ocd() { pkill -9 -x openocd 2>/dev/null || true; sleep 0.4; }

echo "ELF=$ELF"
echo "CFG=$CFG"
ls -la "$ELF" "$CFG"
echo

kill_ocd
"$OCD" -s "$SCRIPTS" -s "$BOARD/openocd" -f "$CFG" >"$OUT/ocd-class4-again.log" 2>&1 &
for i in $(seq 1 20); do ss -ltn | grep -q ':3333' && break; sleep 0.25; done
ss -ltn | grep -E '3333|4444' || echo "WARN: ports not up"

{
  echo 'reset halt'
  sleep 0.3
  echo resume
} | timeout 5 nc 127.0.0.1 4444 >/dev/null 2>&1 || true
sleep 3

# Unrolled CSA walk (8 frames). A11 at +12 (Upper layout word3).
"$DOCKER" run --rm --network host \
  -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
  ulipe-microkernel:dev \
  timeout 50 tricore-elf-gdb -batch /elf/ulmk \
  -ex 'set pagination off' \
  -ex 'set remotetimeout 20' \
  -ex 'target extended-remote :3333' \
  -ex 'monitor halt' \
  -ex 'printf "halt_PC=%p\n", $pc' \
  -ex 'info symbol $pc' \
  -ex 'printf "PCXI=%p FCX=%p LCX=%p PSW=%p\n", $pcx, $fcx, $lcx, $psw' \
  -ex 'set $pcxi=$pcx' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame0: link=0x%x csa=0x%x A11(+12)=0x%x\n", $pcxi, $csa, *(unsigned*)$csa+12' \
  -ex 'printf "frame0: A11=0x%08x\n", *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame1: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame2: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame3: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame4: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame5: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame6: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  -ex 'set $pcxi=*(unsigned*)$csa' \
  -ex 'set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))' \
  -ex 'printf "frame7: link=0x%x csa=0x%x A11=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12)' \
  -ex 'x/16wx $csa' \
  -ex 'info symbol *(unsigned*)($csa+12)' \
  >"$GDB_RAW" 2>&1 || true

echo '=== GDB raw ==='
cat "$GDB_RAW"
echo

echo '=== addr2line every A11(+12) ==='
mapfile -t A11S < <(grep -oE 'A11(=| )0x[0-9a-fA-F]{8}' "$GDB_RAW" | grep -oE '0x[0-9a-fA-F]{8}' || true)
# Prefer A11= lines from printf
mapfile -t A11S < <(grep -E 'A11=0x' "$GDB_RAW" | grep -oE '0x[0-9a-fA-F]{8}' || true)
idx=0
for a in "${A11S[@]}"; do
  # skip link words accidentally matched - A11 should be code in flash ~0xa0......
  sym=$("$DOCKER" run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
    ulipe-microkernel:dev \
    tricore-elf-addr2line -e /elf/ulmk -f -p -a "$a" 2>/dev/null || echo "(addr2line failed)")
  echo "frame$idx A11=$a  =>  $sym"
  idx=$((idx+1))
done

echo
echo '=== halt PC addr2line ==='
halt_pc=$(grep -oE 'halt_PC=0x[0-9a-fA-F]+' "$GDB_RAW" | head -1 | sed 's/.*=//')
if [[ -z "$halt_pc" ]]; then
  halt_pc=$(grep -oE '^0x[0-9a-fA-F]+' "$GDB_RAW" | head -1)
fi
echo "halt_pc=$halt_pc"
if [[ -n "$halt_pc" ]]; then
  "$DOCKER" run --rm -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
    ulipe-microkernel:dev \
    tricore-elf-addr2line -e /elf/ulmk -f -p -a "$halt_pc" 2>/dev/null || true
fi

echo
echo '=== earliest userspace/board fault PC (excluding trap vector / printk / trap_entry) ==='
# Will annotate after listing; filter known noise symbols below.
python3 - <<'PY'
import re, subprocess
raw = open("/home/ulipe/fun/ulmk/.hil_tmp/gdb-class4-again.raw").read()
# Collect A11 from "A11=0x........" lines in frame prints
a11s = re.findall(r"A11(?:=|\(\+12\)=)0x([0-9a-fA-F]+)", raw)
# Also from "frameN: ... A11=0x"
if not a11s:
    a11s = re.findall(r"A11=0x([0-9a-fA-F]{8})", raw)
noise = ("_trap_", "trap_entry", "ulmk_printk", "printk", "_trap_class", "ulmk_arch_trap",
         "ulmk_kern_trap", "trap_vector", "__udiv", "memcpy")
print("candidates (frame order, deepest last):")
chosen = None
for i, h in enumerate(a11s):
    addr = int(h, 16)
    # clear LSB (TriCore function pointer bit)
    addr_c = addr & ~1
    try:
        out = subprocess.check_output([
            "docker","run","--rm",
            "-v","/home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf",
            "ulipe-microkernel:dev",
            "tricore-elf-addr2line","-e","/elf/ulmk","-f","-p","-a",hex(addr)
        ], text=True, stderr=subprocess.DEVNULL).strip()
    except Exception as e:
        out = f"(fail {e})"
    print(f"  frame{i} 0x{h}  {out}")
    low = out.lower()
    is_noise = any(n.lower() in low for n in noise) or "trap_table" in low or "??" in out and addr_c < 0xa0001000
    # Prefer PFlash code outside early trap table
    if chosen is None and not is_noise and (0xa0000000 <= addr_c < 0xa0200000 or 0x80000000 <= addr_c < 0x80200000):
        # further exclude pure trap vector region if symbol says so
        if "_trap_class" not in low and "trap_entry" not in low and "printk" not in low:
            chosen = (hex(addr), out)
# Walk from deepest (later frames) toward trap for "earliest" fault site:
# Earliest fault = first non-noise frame when walking from CURRENT toward older chain
# (frame0 is newest / trap side). So first non-noise after frame0 trap frames.
chosen2 = None
for i, h in enumerate(a11s):
    addr = int(h, 16)
    try:
        out = subprocess.check_output([
            "docker","run","--rm",
            "-v","/home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf",
            "ulipe-microkernel:dev",
            "tricore-elf-addr2line","-e","/elf/ulmk","-f","-p","-a",hex(addr)
        ], text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        continue
    low = out.lower()
    if any(n.lower() in low for n in noise):
        continue
    if "trap_table" in low or "_trap_" in low:
        continue
    if "printk" in low:
        continue
    chosen2 = (hex(addr), out)
    break
print()
if chosen2:
    print(f"EARLIEST_USER_OR_BOARD: {chosen2[0]}  {chosen2[1]}")
else:
    print("EARLIEST_USER_OR_BOARD: (none found)")
PY

kill_ocd
echo
echo DONE_CLASS4_AGAIN
