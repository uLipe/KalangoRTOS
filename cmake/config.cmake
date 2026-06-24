# cmake/config.cmake
# Kernel static configuration symbols.
# Full specification: docs/build_system_spec.md §9
#
# Include this file from the top-level CMakeLists.txt before any targets.
# The five symbols are exposed as CMake cache variables so the integrator
# can override them on the command line (-DUL_CONFIG_MAX_THREADS=32).

set(UL_CONFIG_MAX_THREADS      16   CACHE STRING "Max simultaneously-existing threads")
set(UL_CONFIG_MAX_ENDPOINTS    32   CACHE STRING "Max simultaneously-existing IPC endpoints")
set(UL_CONFIG_MAX_NOTIFS       32   CACHE STRING "Max simultaneously-existing notification objects")
set(UL_CONFIG_MAX_IRQ_BINDINGS 16   CACHE STRING "Max IRQ-to-notification bindings")
set(UL_CONFIG_TICK_HZ          1000 CACHE STRING "Kernel tick frequency in Hz")

# Debug printk: 1 = enabled (default for debug builds), 0 = eliminated at compile time.
set(UL_CONFIG_DEBUG_PRINTK     1    CACHE STRING "Enable kernel printk (0 = production no-op)")

# Validate ranges at configure time; fail fast rather than at runtime.
foreach(sym IN ITEMS
        UL_CONFIG_MAX_THREADS
        UL_CONFIG_MAX_ENDPOINTS
        UL_CONFIG_MAX_NOTIFS
        UL_CONFIG_MAX_IRQ_BINDINGS)
    if("${${sym}}" LESS 1 OR "${${sym}}" GREATER 256)
        message(FATAL_ERROR "${sym}=${${sym}} out of range [1, 256]")
    endif()
endforeach()

if("${UL_CONFIG_TICK_HZ}" LESS 1 OR "${UL_CONFIG_TICK_HZ}" GREATER 10000)
    message(FATAL_ERROR "UL_CONFIG_TICK_HZ=${UL_CONFIG_TICK_HZ} out of range [1, 10000]")
endif()
