/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * include/ulmk/syscall_abi.h — arch-agnostic redirector
 *
 * This header contains no code of its own.  It delegates to the
 * architecture-specific implementation found via the build system's
 * arch include path (e.g. -Iarch/tricore/include for TriCore).
 *
 * Pattern mirrors Linux <asm/...> headers:
 *   include/ulmk/syscall_abi.h  →  arch/<arch>/include/ulmk_syscall_abi.h
 *
 * When porting to a new arch, provide:
 *   arch/<newarch>/include/ulmk_syscall_abi.h
 * with the same ULMK_SYSCALL_N() macros and result-struct definitions.
 */

#ifndef ULMK_SYSCALL_ABI_H
#define ULMK_SYSCALL_ABI_H

#include <ulmk_syscall_abi.h>

#endif /* ULMK_SYSCALL_ABI_H */
