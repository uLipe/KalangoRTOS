set remotetimeout 60
target extended-remote :3333
monitor reset halt
monitor resume
shell sleep 5
monitor halt
printf "pc=%p\n", (void*)$pc
printf "send=%u isr=%u seen=%u\n", (unsigned)*(volatile uint32_t*)&g_ulmk_ipi_send_count, (unsigned)*(volatile uint32_t*)&g_ulmk_ipi_isr_count, (unsigned)*(volatile uint32_t*)&g_seen_cpu1
printf "armed=%u %u\n", (unsigned)g_secondary_armed[0], (unsigned)g_secondary_armed[1]
printf "online=%u %u needs=%u %u\n", (unsigned)g_ulmk_percpu[0].online, (unsigned)g_ulmk_percpu[1].online, (unsigned)g_ulmk_percpu[0].needs_resched, (unsigned)g_ulmk_percpu[1].needs_resched
printf "cur0=%p cur1=%p\n", g_ulmk_percpu[0].current, g_ulmk_percpu[1].current
printf "idle0 cpu=%u ready=%u pcxi=%x\n", (unsigned)idle_thread_g[0].cpu, (unsigned)idle_thread_g[0].ctx_ready, (unsigned)idle_thread_g[0].ctx.pcxi
printf "idle1 cpu=%u ready=%u pcxi=%x\n", (unsigned)idle_thread_g[1].cpu, (unsigned)idle_thread_g[1].ctx_ready, (unsigned)idle_thread_g[1].ctx.pcxi
# Walk TCB registry
set $n = &tcb_registry
printf "registry head=%p\n", *(void**)$n
# Each node is sys_dnode at offset of reg_node — find offsetof. Use nm/readelf
# Dump 4 TCBs from idle + root + search heap for cpu==1
printf "root cpu=%u state=%u\n", (unsigned)root_thread_g.cpu, (unsigned)root_thread_g.state
# Scan typical heap TCBs after root — print first heap object as TCB-like
# From previous knowledge heap at user_pool
x/wx &_ulmk_user_pool_start
set $p = (char*)*(unsigned*)&_ulmk_user_pool_start
# TLSF often puts block after header — dump words looking for entry a00072ac
find /w &_ulmk_user_pool_start, +0x8000, 0xa00072ac
printf "GPSR10=%x ICR1=%x PC1=%x DBGSR1=%x\n", *(unsigned*)0xF0039020, *(unsigned*)0xF883FE2C, *(unsigned*)0xF883FE08, *(unsigned*)0xF883FD00
bt 5
