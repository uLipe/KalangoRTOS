set pagination off
set remotetimeout 30
target extended-remote :3333
monitor halt
printf "halt_PC=%p PCXI=%p FCX=%p LCX=%p\n", $pc, $pcx, $fcx, $lcx
set $pcxi=$pcx
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F0 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F1 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F2 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F3 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F4 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F5 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F6 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F7 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F8 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F9 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F10 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F11 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F12 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F13 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F14 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F15 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F16 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F17 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F18 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F19 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F20 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F21 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F22 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F23 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F24 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F25 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F26 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F27 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F28 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F29 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F30 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F31 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F32 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F33 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F34 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F35 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F36 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F37 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F38 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F39 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F40 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F41 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F42 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F43 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F44 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F45 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F46 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F47 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F48 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F49 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F50 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F51 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F52 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F53 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F54 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F55 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F56 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F57 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F58 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F59 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F60 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F61 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F62 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F63 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F64 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F65 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F66 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F67 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F68 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F69 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F70 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F71 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F72 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F73 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F74 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F75 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F76 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F77 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F78 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F79 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F80 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F81 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F82 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F83 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F84 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F85 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F86 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F87 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F88 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F89 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F90 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F91 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F92 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F93 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F94 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F95 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F96 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F97 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F98 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F99 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F100 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F101 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F102 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F103 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F104 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F105 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F106 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F107 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F108 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F109 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F110 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F111 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F112 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F113 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F114 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F115 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F116 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F117 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F118 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F119 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F120 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F121 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F122 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F123 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F124 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F125 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F126 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F127 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F128 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F129 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F130 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F131 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F132 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F133 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F134 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F135 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F136 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F137 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F138 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F139 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F140 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F141 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F142 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F143 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F144 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F145 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F146 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F147 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F148 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F149 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F150 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F151 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F152 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F153 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F154 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F155 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F156 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F157 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F158 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F159 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F160 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F161 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F162 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F163 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F164 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F165 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F166 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F167 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F168 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F169 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F170 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F171 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F172 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F173 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F174 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F175 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F176 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F177 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F178 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F179 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F180 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F181 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F182 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F183 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F184 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F185 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F186 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F187 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F188 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F189 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F190 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F191 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F192 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F193 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F194 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F195 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F196 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F197 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F198 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F199 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F200 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F201 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F202 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F203 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F204 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F205 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F206 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F207 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F208 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F209 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F210 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F211 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F212 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F213 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F214 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F215 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F216 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F217 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F218 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F219 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F220 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F221 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F222 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F223 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F224 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F225 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F226 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F227 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F228 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F229 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F230 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F231 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F232 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F233 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F234 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F235 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F236 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F237 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F238 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F239 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F240 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F241 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F242 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F243 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F244 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F245 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F246 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F247 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F248 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F249 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F250 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F251 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F252 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F253 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F254 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
set $csa=((($pcxi&0x70000)<<12)|(($pcxi&0xffff)<<6))
printf "F255 link=%08x csa=%08x a11=%08x a10=%08x psw=%08x\n", $pcxi, $csa, *(unsigned*)($csa+12), *(unsigned*)($csa+8), *(unsigned*)($csa+4)
set $pcxi=*(unsigned*)$csa
