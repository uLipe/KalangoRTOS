set pagination off
target remote :3333
printf "PC=%p D15=%p PCX=%p FCX=%p PSW=%p A11=%p\n",$pc,$d15,$pcx,$fcx,$psw,$a11
set $link = (unsigned long)$pcx
set $i = 0
while ($i < 24)
  if ($link == 0)
    loop_break
  end
  set $a = ((($link & 0x70000) << 12) | (($link & 0xffff) << 6))
  set $w0 = *(unsigned int*)$a
  set $w1 = *(unsigned int*)($a+4)
  set $w2 = *(unsigned int*)($a+8)
  set $w3 = *(unsigned int*)($a+12)
  set $w11 = *(unsigned int*)($a+44)
  set $w15 = *(unsigned int*)($a+60)
  printf "CSA[%d] @%p link_in=%p next=%p\n",$i,$a,$link,$w0
  printf "  w1=%p w2=%p w3=%p w11=%p w15=%p\n",$w1,$w2,$w3,$w11,$w15
  set $link = (unsigned long)$w0
  set $i = $i + 1
end
detach
quit
