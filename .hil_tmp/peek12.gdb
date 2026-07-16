set remotetimeout 60
target extended-remote :3333
monitor reset halt
break silicon_smp_smoke_done
break ulmk_kern_trap_panic
break ulmk_arch_ipi_soft_take
commands
silent
printf "SOFT_TAKE on cpu? d2 later\n"
continue
end
continue
