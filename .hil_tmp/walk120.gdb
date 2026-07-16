set pagination off
set remotetimeout 25
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
