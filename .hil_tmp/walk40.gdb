set pagination off
set remotetimeout 20
target extended-remote :3333
monitor halt
printf "halt_PC=%p\n", $pc
info symbol $pc
printf "PCXI=%p FCX=%p LCX=%p\n", $pcx, $fcx, $lcx
set $pcxi=$pcx
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame0: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame1: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame2: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame3: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame4: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame5: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame6: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame7: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame8: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame9: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame10: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame11: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame12: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame13: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame14: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame15: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame16: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame17: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame18: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame19: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame20: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame21: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame22: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame23: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame24: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame25: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame26: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame27: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame28: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame29: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame30: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame31: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame32: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame33: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame34: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame35: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame36: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame37: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame38: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "frame39: link=0x%08x csa=0x%08x A11_12=0x%08x A11_4=0x%08x w1=0x%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+4), *(unsigned*)($csa+4)
info symbol *(unsigned*)($csa+12)
set $pcxi=*(unsigned*)$csa
