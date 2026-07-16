set remotetimeout 60
target extended-remote :3333
monitor reset halt
monitor resume
shell sleep 5
monitor halt
printf "==== CPU0 PC ====\n"
printf "pc=%p\n", (void*)$pc
bt 6
printf "==== seen / ipi / armed ====\n"
x/wx &g_seen_cpu1
x/wx &g_ulmk_ipi_isr_count
x/wx &g_ulmk_ipi_send_count
x/2wx &g_secondary_armed
printf "==== GPSR00/10 ====\n"
x/wx 0xF0039000
x/wx 0xF0039020
printf "==== CPU1 CSFR PCXI FCX ICR DBGSR ====\n"
x/wx 0xF883FE00
x/wx 0xF883FE38
x/wx 0xF883FE2C
x/wx 0xF883FD00
printf "==== CPU1 PC reg ====\n"
x/wx 0xF883FE08
printf "==== percpu[0] and [1] (guess layout) ====\n"
x/16wx &g_ulmk_percpu
printf "==== console ====\n"
x/80cb &g_ulmk_console_log
