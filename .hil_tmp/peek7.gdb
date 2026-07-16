set remotetimeout 60
target extended-remote :3333
monitor reset halt
break silicon_smp_smoke_done
break ulmk_kern_trap_panic
continue
# if timeout - interrupt: not, use timeout wrapper
printf "pc=%p\n", (void*)$pc
printf "scratch=%x\n", *(unsigned*)&g_ulmk_board_hil_scratch
printf "send=%u isr=%u seen=%u armed1=%u\n", *(unsigned*)0x70002bf0, *(unsigned*)0x70002bf4, *(unsigned*)&g_seen_cpu1, ((unsigned*)&g_secondary_armed)[1]
printf "online0=%u online1=%u\n", (unsigned)g_ulmk_percpu[0].online, (unsigned)g_ulmk_percpu[1].online
x/48cb &g_ulmk_console_log
bt 4
