#!/bin/bash
exec > /home/ulipe/fun/ulmk/.hil_tmp/attach-run2.out 2>&1
set +x
export PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/home/ulipe/.local/aurix/bin
DOCKER=$(command -v docker || true)
if [ -z "$DOCKER" ]; then
  for d in /usr/bin/docker /usr/local/bin/docker; do
    [ -x "$d" ] && DOCKER=$d && break
  done
fi

echo "docker=$DOCKER"
echo "=== telnet full ==="
printf 'halt\nreg PC\nreg PCX\nreg PSW\nreg a10\nreg a11\nreg d15\nmdw 0x70009480 4\nmdw 0x70009488 64\nexit\n' | timeout 8 nc 127.0.0.1 4444 | tr -d '\000\033' | tee /home/ulipe/fun/ulmk/.hil_tmp/ocd-telnet3.txt

echo "=== gdb docker ==="
if [ -n "$DOCKER" ]; then
  "$DOCKER" run --rm --network host \
    -v /home/ulipe/fun/build/ulipe-tricore-tc275_lite:/elf \
    -v /home/ulipe/fun/ulmk/.hil_tmp:/out \
    ulipe-microkernel:dev \
    timeout 60 tricore-elf-gdb -batch /elf/ulmk \
    -ex 'set pagination off' \
    -ex 'set remotetimeout 30' \
    -ex 'target extended-remote :3333' \
    -ex 'monitor halt' \
    -ex 'info registers' \
    -ex 'printf "PC=%p\n", $PC' \
    -ex 'info symbol $PC' \
    -ex 'x/8i $PC' \
    -ex 'bt 15' \
    | tee /home/ulipe/fun/ulmk/.hil_tmp/gdb-attach.out
else
  echo 'NO DOCKER'
fi

echo DONE2
