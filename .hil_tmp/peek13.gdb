set remotetimeout 60
target extended-remote :3333
monitor reset halt
break silicon_smp_smoke_done
break ulmk_kern_trap_panic
continue
