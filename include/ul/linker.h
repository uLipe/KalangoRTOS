/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Linker C macro API — ulipeMicroKernel
 * Full specification: docs/linker_spec.md §8
 *
 * Usage: #include <ul/linker.h>
 */

#ifndef UL_LINKER_H
#define UL_LINKER_H

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>	/* ul_domain_desc_t, UL_PERM_* */

/*
 * Place a zero-initialised variable in the named domain's bss section.
 * The linker collects all UL_DOMAIN_BSS(foo) vars into .domain_foo.bss.
 */
#define UL_DOMAIN_BSS(name) \
	__attribute__((section(".domain_" #name ".bss")))

/*
 * Place an initialised variable in the named domain's data section.
 * Requires the linker to emit an AT > KERNEL_FLASH LMA for this section,
 * which generate_ld.py ensures in the domain_data.ld.in snippet.
 */
#define UL_DOMAIN_DATA(name) \
	__attribute__((section(".domain_" #name ".data")))

/*
 * Shorthand: place in the current module's domain.
 * Requires: #define UL_MODULE_NAME <name> before including this header.
 */
#define UL_PRIVATE		UL_DOMAIN_BSS(UL_MODULE_NAME)
#define UL_PRIVATE_INIT		UL_DOMAIN_DATA(UL_MODULE_NAME)

/*
 * Register a domain descriptor in .domain_table (flash, read-only).
 * The kernel scans _ul_domain_table_start … _ul_domain_table_end at boot.
 *
 * Expands to:
 *   - extern declarations for the _ul_domain_NAME_start/end linker symbols
 *   - a static const ul_domain_desc_t placed in .domain_table with 'used'
 */
#define UL_DEFINE_DOMAIN(dname, perms)					\
	extern uint8_t _ul_domain_##dname##_start[];			\
	extern uint8_t _ul_domain_##dname##_end[];			\
	static const ul_domain_desc_t __ul_domain_desc_##dname		\
		__attribute__((section(".domain_table"), used)) = {	\
		.name  = #dname,					\
		.start = (uintptr_t)_ul_domain_##dname##_start,	\
		.end   = (uintptr_t)_ul_domain_##dname##_end,		\
		.perms = (perms),					\
	}

/*
 * Redirect a function or object to the app-specific code section so the
 * linker script's app_code snippet can isolate it for MPU enforcement.
 *
 * Set by the build system via -DUL_APP_NAME=<name> for each app's TUs.
 * If UL_APP_NAME is not defined (kernel code), these expand to nothing.
 */
#ifdef UL_APP_NAME
#define UL_APP_TEXT \
	__attribute__((section(".text." UL_LINKER_STR(UL_APP_NAME) "." __func__)))
#define UL_APP_RODATA \
	__attribute__((section(".rodata." UL_LINKER_STR(UL_APP_NAME))))
#else
#define UL_APP_TEXT
#define UL_APP_RODATA
#endif

/* Internal stringification helpers */
#define UL_LINKER_STR_(x)	#x
#define UL_LINKER_STR(x)	UL_LINKER_STR_(x)

#endif /* UL_LINKER_H */
