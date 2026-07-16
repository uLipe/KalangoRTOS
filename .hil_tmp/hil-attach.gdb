set remotetimeout 60
target extended-remote :3333
monitor halt
printf "attached\n"
info registers pc
bt 12
x/wx &g_ulmk_board_hil_scratch
x/wx &g_ulmk_console_log_len
x/1024cb &g_ulmk_console_log
