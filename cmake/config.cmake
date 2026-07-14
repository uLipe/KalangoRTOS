# cmake/config.cmake
# Kernel static configuration — user-tunable knobs exposed as cache variables.
# Canonical defaults and validation live in tools/gen_config.py (the single
# generator used by both this build and the integration-test Makefiles).
# Full specification: docs/build_system_spec.md §10

set(ULMK_CONFIG_MAX_IRQ_BINDINGS 16 CACHE STRING "Max IRQ-to-notification bindings")
set(ULMK_CONFIG_DEBUG_PRINTK     1  CACHE STRING "Enable kernel printk (0 = production no-op)")
set(ULMK_CONFIG_SYSCALL_WCET     0  CACHE STRING
	"Syscall cycle-counter slot (0=off, 1=WCET HIL / silicon_wcet)")
