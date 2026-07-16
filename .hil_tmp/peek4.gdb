set remotetimeout 60
target extended-remote :3333
monitor reset halt
break silicon_smp_smoke_done
break ulmk_kern_trap_panic
break ulmk_arch_send_ipi
commands
silent
printf "SEND_IPI cpu=%u from cpu_id\n", $r4
continue
end
break ulmk_sched_enqueue
commands
silent
# a4 = thread*
printf "ENQ t=%p cpu=%u prio=%u\n", $a4, *(unsigned char*)($a4+29), *(unsigned char*)($a4+28)
continue
end
continue
# if we hit done or panic or timeout via external - after continue returns...
printf "stopped pc=%p\n", (void*)$pc
printf "send=%u isr=%u seen=%u\n", *(unsigned*)0x70002bf0, *(unsigned*)0x70002bf4, *(unsigned*)0x70000010
printf "online bytes: "
x/4xb 0x70001480
x/4xb 0x70001498
x/64cb 0x70009448
bt 4
