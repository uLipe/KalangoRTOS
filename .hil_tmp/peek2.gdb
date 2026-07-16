set remotetimeout 60
target extended-remote :3333
monitor reset halt
monitor resume
shell sleep 4
monitor halt
printf "send=%u isr=%u seen=%u\n", g_ulmk_ipi_send_count, g_ulmk_ipi_isr_count, g_seen_cpu1
printf "armed0=%u armed1=%u\n", g_secondary_armed[0], g_secondary_armed[1]
printf "pc0=%p\n", (void*)$pc
# idle[0] and idle[1] cpu field — offsetof cpu in TCB: after ctx(4)+stack_base(4)+stack_size(4)+slab(4)+slab_size(4)+heap_base(4)+heap_size(4)+prio(1)=29?
# dump idle_thread_g (2 threads)
x/64wx &idle_thread_g
# dump percpu online bytes explicitly
x/2xb ((char*)&g_ulmk_percpu)+16
x/2xb ((char*)&g_ulmk_percpu)+40
# Find TCBs: search user heap area for tid pattern — easier: walk from root stack
# Print first heap allocations - dump g_ulmk_percpu currents
printf "cur0=%p cur1=%p\n", g_ulmk_percpu[0].current, g_ulmk_percpu[1].current
printf "online0=%d online1=%d needs0=%d needs1=%d\n", g_ulmk_percpu[0].online, g_ulmk_percpu[1].online, g_ulmk_percpu[0].needs_resched, g_ulmk_percpu[1].needs_resched
# worker: scan registry if we can find symbol - try printing *(cpu) for several heap TCBs
# heap starts after BSS — use board: _ulmk_user_pool
printf "idle0.cpu=%u idle1.cpu=%u\n", idle_thread_g[0].cpu, idle_thread_g[1].cpu
printf "idle0.ctx=%x idle1.ctx=%x ready0=%u ready1=%u\n", idle_thread_g[0].ctx.pcxi, idle_thread_g[1].ctx.pcxi, idle_thread_g[0].ctx_ready, idle_thread_g[1].ctx_ready
bt 8
