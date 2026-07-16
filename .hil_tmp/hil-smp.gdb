set remotetimeout 60
target extended-remote :3333
monitor reset halt
break ulmk_board_cpu_start
continue
printf "at board_cpu_start\n"
monitor resume
shell sleep 5
monitor halt
printf "after 5s halt\n"
info registers pc
bt 12
x/wx &g_ulmk_board_hil_scratch
x/wx &g_ulmk_console_log_len
x/512cb &g_ulmk_console_log
