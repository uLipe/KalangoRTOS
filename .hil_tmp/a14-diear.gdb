set pagination off
set remotetimeout 15
target extended-remote :3333
printf "PC=%p\n", $pc
# CPU0 Class4: DATR/DIEAR/DIETR — try several known maps
set $addrs[0]=0xF7E1A010
set $addrs[1]=0xF7E1A014
set $addrs[2]=0xF7E1A018
set $i=0
while $i < 3
  printf "mmio 0x%08x = 0x%08x\n", $addrs[$i], *(unsigned*)$addrs[$i]
  set $i = $i + 1
end
# also try DSEAR style
x/4xw 0xF7E19080
# root_stack symbol
printf "root_stack:\n"
info address root_stack_g
p/x &root_stack_g
p/x sizeof(root_stack_g)
detach
quit
