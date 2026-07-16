set remotetimeout 60
target extended-remote :3333
monitor reset halt
break silicon_smp_smoke_done
continue
