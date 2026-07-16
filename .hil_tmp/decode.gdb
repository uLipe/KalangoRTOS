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
