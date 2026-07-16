set pagination off
set remotetimeout 15
target extended-remote :3333
x/1xw 0xF7E19080
x/1xw 0xF7E19084
x/1xw 0xF7E1A010
x/1xw 0xF7E1A014
x/1xw 0xF7E1A018
x/1xw 0xF000E010
# PSW from CSA
printf "PSW=0x%08x IO=%u PRS=%u\n", 0x18001584, (0x18001584>>10)&3, (0x18001584>>14)&3
# stack bounds
printf "root_stack_g=0x70007280 top~0x70008280 store=0x7000822c\n"
x/8xw 0x70008220
detach
quit
