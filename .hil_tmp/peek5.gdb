set remotetimeout 60
target extended-remote :3333
monitor reset halt
break ulmk_kern_thread_spawn
commands
silent
printf "SPAWN uattr=%p cpu_byte[28]=%u stack=%u prio=%u priv=%u heap=%u\n", $a4, *(unsigned char*)($a4+28), *(unsigned*)($a4+16), *(unsigned char*)($a4+12), *(unsigned*)($a4+20), *(unsigned*)($a4+24)
printf "raw attr: "; x/8wx $a4
continue
end
break worker_cpu1
commands
printf "WORKER ran! cpu_id next\n"
continue
end
break silicon_smp_smoke_done
continue
printf "done pc=%p send=%u seen=%u\n", (void*)$pc, *(unsigned*)0x70002bf0, *(unsigned*)0x70000010
x/40cb 0x70009448
