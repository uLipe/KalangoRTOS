set pagination off
set remotetimeout 20
target extended-remote :3333
printf "PC=0x%lx\n", (unsigned long)$pc
printf "CSA@0x70002cc0:\n"
x/16xw 0x70002cc0
printf "A10(SP)=0x%08x A11(RA)=0x%08x A14=0x%08x A15=0x%08x\n", *(unsigned*)0x70002cc8, *(unsigned*)0x70002ccc, *(unsigned*)0x70002ce8, *(unsigned*)0x70002cec
printf "store_addr A14-4=0x%08x\n", (*(unsigned*)0x70002ce8)-4
x/4xw ((*(unsigned*)0x70002ce8)-4)
# walk prev from PCXI word0
set $link = *(unsigned*)0x70002cc0
set $i=0
while ($i < 4)
  set $a = (($link & 0x70000)<<12) | (($link & 0xffff)<<6)
  printf "prev[%d] link=0x%x addr=0x%x\n", $i, $link, $a
  x/16xw $a
  set $link = *(unsigned*)$a
  set $i = $i + 1
  if ($link & 0xfffff) == 0
    loop_break
  end
end
detach
quit
