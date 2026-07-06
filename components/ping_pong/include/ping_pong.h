/* SPDX-License-Identifier: MIT */
/*
 * ping_pong component public API — components/ping_pong/include/ping_pong.h
 */

#ifndef PING_PONG_H
#define PING_PONG_H

#include <ulmk/microkernel.h>

ulmk_tid_t ping_pong_init(const ulmk_boot_info_t *info);

#endif /* PING_PONG_H */
