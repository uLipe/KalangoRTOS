set remotetimeout 60
target extended-remote :3333
monitor reset halt
break *ulmk_kern_thread_spawn
commands
silent
set $ua = (unsigned char *)$d4
printf "SPAWN uattr=%p cpu=%u stack=%u prio=%u priv=%u heap=%u entry=%p\n", $ua, (unsigned)$ua[28], *(unsigned*)($ua+16), (unsigned)$ua[12], *(unsigned*)($ua+20), *(unsigned*)($ua+24), *(void**)($ua+4)
continue
end
break *ulmk_sched_enqueue
commands
silent
set $t = (unsigned char *)$a4
printf "ENQ t=%p cpu=%u prio=%u entry_via_start?\n", $t, (unsigned)$t[29], (unsigned)$t[28]
continue
end
break silicon_smp_smoke_done
break ulmk_kern_trap_panic
continue
printf "STOP pc=%p send=%u isr=%u seen=%u\n", (void*)$pc, *(unsigned*)0x70002bf0, *(unsigned*)0x70002bf4, *(unsigned*)0x70000010
x/48cb 0x70009448
bt 3
