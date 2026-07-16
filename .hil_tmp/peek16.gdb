set remotetimeout 90
target extended-remote :3333
monitor reset halt
break silicon_smp_smoke_done
# After spawn, break on send_ipi then later dump
break ulmk_arch_send_ipi
commands
silent
printf "SEND_IPI cpu arg in d4? peek soft after\n"
continue
end
break ulmk_kern_ipi_from_isr
commands
silent
printf "IPI_FROM_ISR entered\n"
continue
end
break ulmk_arch_ipi_soft_take if g_ulmk_ipi_soft[1] != 0
commands
silent
printf "SOFT_TAKE with soft1 pending\n"
continue
end
continue
