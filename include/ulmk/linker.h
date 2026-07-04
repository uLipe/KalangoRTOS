/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Linker C macro API — ulmk
 * Full specification: docs/linker_spec.md §8
 *
 * Usage: #include <ulmk/linker.h>
 */

#ifndef ULMK_LINKER_H
#define ULMK_LINKER_H

#include <stdint.h>
#include <stddef.h>
#include <ulmk/microkernel.h>	/* ulmk_domain_desc_t, UL_PERM_* */

/*
 * Stringification helpers — must come before any macro that uses them so
 * the replacement list is resolved correctly at expansion time.
 *
 * UL_LINKER_STR(X) expands X first, then stringifies the result.
 * This is needed when the argument is itself a macro (e.g. ULMK_MODULE_NAME).
 */
#define UL_LINKER_STR_(x)	#x
#define UL_LINKER_STR(x)	UL_LINKER_STR_(x)

/*
 * Place a zero-initialised variable in the named domain's bss section.
 * The linker collects all ULMK_DOMAIN_BSS(foo) vars into .domain_foo.bss.
 *
 * UL_LINKER_STR(name) is used so that when name is itself a macro (e.g.
 * ULMK_MODULE_NAME defined via -D), it is expanded to its value before
 * stringification.
 */
#define ULMK_DOMAIN_BSS(name) \
	__attribute__((section(".domain_" UL_LINKER_STR(name) ".bss")))

/*
 * Place an initialised variable in the named domain's data section.
 * Requires the linker to emit an AT > KERNEL_FLASH LMA for this section,
 * which generate_ld.py ensures in the domain_data.ld.in snippet.
 */
#define ULMK_DOMAIN_DATA(name) \
	__attribute__((section(".domain_" UL_LINKER_STR(name) ".data")))

/*
 * Shorthand: place in the current module's domain.
 * Requires: -DULMK_MODULE_NAME=<name> in the component's compile flags.
 */
#define ULMK_PRIVATE		ULMK_DOMAIN_BSS(ULMK_MODULE_NAME)
#define ULMK_PRIVATE_INIT	ULMK_DOMAIN_DATA(ULMK_MODULE_NAME)

/*
 * Register a domain descriptor in .domain_table (flash, read-only).
 * The kernel scans _ulmk_domain_table_start … _ulmk_domain_table_end at boot.
 *
 * Expands to:
 *   - extern declarations for the _ulmk_domain_NAME_start/end linker symbols
 *   - a static const ulmk_domain_desc_t placed in .domain_table with 'used'
 */
#define ULMK_DEFINE_DOMAIN(dname, perms)					\
	extern uint8_t _ulmk_domain_##dname##_start[];			\
	extern uint8_t _ulmk_domain_##dname##_end[];			\
	static const ulmk_domain_desc_t __ulmk_domain_desc_##dname		\
		__attribute__((section(".domain_table"), used)) = {	\
		.name  = #dname,					\
		.start = (uintptr_t)_ulmk_domain_##dname##_start,	\
		.end   = (uintptr_t)_ulmk_domain_##dname##_end,		\
		.perms = (perms),					\
	}

/*
 * Redirect a function or object to the app-specific code section so the
 * linker script's app_code snippet can isolate it for MPU enforcement.
 *
 * Set by the build system via -DULMK_APP_NAME=<name> for each app's TUs.
 * If ULMK_APP_NAME is not defined (kernel code), these expand to nothing.
 */
#ifdef ULMK_APP_NAME
#define UL_APP_TEXT \
	__attribute__((section(".text." UL_LINKER_STR(ULMK_APP_NAME) "." __func__)))
#define UL_APP_RODATA \
	__attribute__((section(".rodata." UL_LINKER_STR(ULMK_APP_NAME))))
#else
#define UL_APP_TEXT
#define UL_APP_RODATA
#endif

#endif /* ULMK_LINKER_H */
