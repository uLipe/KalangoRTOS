/* SPDX-License-Identifier: MIT */
/* boards/qemu_mps2_an500/board_internal.h — shared board IPC endpoint */

#ifndef BOARD_INTERNAL_H
#define BOARD_INTERNAL_H

#include <ulmk/microkernel.h>

ulmk_ep_t board_service_ep(void);

#endif /* BOARD_INTERNAL_H */
