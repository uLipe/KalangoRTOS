set remotetimeout 60
target extended-remote :3333
monitor reset halt
monitor resume
shell sleep 6
monitor halt
printf "pc=%p scratch=0x%x\n", (void*)$pc, *(unsigned*)0x70009440
printf "send=%u isr=%u seen=%u\n", *(unsigned*)0x70002bf0, *(unsigned*)0x70002bf4, *(unsigned*)0x70000010
printf "armed=%u %u\n", *(unsigned*)0x70002c00, *(unsigned*)0x70002c04
x/64cb 0x70009448
bt 5
