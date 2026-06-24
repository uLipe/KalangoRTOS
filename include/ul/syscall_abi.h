/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * include/ul/syscall_abi.h — arch-agnostic redirector
 *
 * This header contains no code of its own.  It delegates to the
 * architecture-specific implementation found via the build system's
 * arch include path (e.g. -Iarch/tricore/include for TriCore).
 *
 * Pattern mirrors Linux <asm/...> headers:
 *   include/ul/syscall_abi.h  →  arch/<arch>/include/ul_syscall_abi.h
 *
 * When porting to a new arch, provide:
 *   arch/<newarch>/include/ul_syscall_abi.h
 * with the same UL_SYSCALL_N() macros and result-struct definitions.
 */

#ifndef UL_SYSCALL_ABI_H
#define UL_SYSCALL_ABI_H

#include <ul_syscall_abi.h>

#endif /* UL_SYSCALL_ABI_H */
