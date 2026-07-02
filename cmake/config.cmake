# cmake/config.cmake
# Kernel static configuration symbols.
# Full specification: docs/build_system_spec.md §9
#
# Include this file from the top-level CMakeLists.txt before any targets.
# The five symbols are exposed as CMake cache variables so the integrator
# can override them on the command line (-DULMK_CONFIG_MAX_THREADS=32).

set(ULMK_CONFIG_MAX_THREADS      16   CACHE STRING "Max simultaneously-existing threads")
set(ULMK_CONFIG_MAX_ENDPOINTS    32   CACHE STRING "Max simultaneously-existing IPC endpoints")
set(ULMK_CONFIG_MAX_NOTIFS       32   CACHE STRING "Max simultaneously-existing notification objects")
set(ULMK_CONFIG_MAX_IRQ_BINDINGS 16   CACHE STRING "Max IRQ-to-notification bindings")
set(ULMK_CONFIG_TICK_HZ          1000 CACHE STRING "Kernel tick frequency in Hz")

# Debug printk: 1 = enabled (default for debug builds), 0 = eliminated at compile time.
set(ULMK_CONFIG_DEBUG_PRINTK     1    CACHE STRING "Enable kernel printk (0 = production no-op)")

# Scheduler time-slice quantum in microseconds (default 10 ms).
set(ULMK_CONFIG_SCHED_QUANTUM_US 10000 CACHE STRING "Preemptive scheduler time-slice quantum in µs")

# Machine clock frequency in Hz — the input clock to the arch tick timer.
# The tick timer peripheral varies per architecture; this is the frequency
# of whatever counter the arch port uses to generate kernel ticks.
# Must match the board PLL/clock configuration.  Always override per-board.
# 50 MHz matches the QEMU TC397B STM0 simulation rate used in all tests.
set(ULMK_CONFIG_HW_SYS_CLOCK_HZ 50000000 CACHE STRING "Machine clock frequency fed to the arch tick timer (Hz)")

# Validate ranges at configure time; fail fast rather than at runtime.
foreach(sym IN ITEMS
        ULMK_CONFIG_MAX_THREADS
        ULMK_CONFIG_MAX_ENDPOINTS
        ULMK_CONFIG_MAX_NOTIFS
        ULMK_CONFIG_MAX_IRQ_BINDINGS)
    if("${${sym}}" LESS 1 OR "${${sym}}" GREATER 256)
        message(FATAL_ERROR "${sym}=${${sym}} out of range [1, 256]")
    endif()
endforeach()

if("${ULMK_CONFIG_TICK_HZ}" LESS 1 OR "${ULMK_CONFIG_TICK_HZ}" GREATER 10000)
    message(FATAL_ERROR "ULMK_CONFIG_TICK_HZ=${ULMK_CONFIG_TICK_HZ} out of range [1, 10000]")
endif()
