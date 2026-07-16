set remotetimeout 60
target extended-remote :3333
monitor reset halt
# gdb continue runs; then we rely on timeout+second session
break *ulmk_heap_alloc
commands
silent
printf "HEAP_ALLOC enter\n"
continue
end
break silicon_smp_smoke_done
break ulmk_kern_trap_panic
continue
