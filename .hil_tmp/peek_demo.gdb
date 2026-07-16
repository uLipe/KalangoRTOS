set remotetimeout 60
target extended-remote :3333
monitor reset halt
break ulmk_kern_trap_panic
# Let it run a few seconds
continue &
shell sleep 8
interrupt
