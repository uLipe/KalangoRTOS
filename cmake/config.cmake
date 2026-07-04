# cmake/config.cmake
# Kernel static configuration symbols.
# Full specification: docs/build_system_spec.md §9

set(ULMK_CONFIG_MAX_THREADS      16   CACHE STRING "Max simultaneously-existing threads")
set(ULMK_CONFIG_MAX_ENDPOINTS    32   CACHE STRING "Max simultaneously-existing IPC endpoints")
set(ULMK_CONFIG_MAX_NOTIFS       32   CACHE STRING "Max simultaneously-existing notification objects")
set(ULMK_CONFIG_MAX_IRQ_BINDINGS 16   CACHE STRING "Max IRQ-to-notification bindings")

set(ULMK_CONFIG_DEBUG_PRINTK     1    CACHE STRING "Enable kernel printk (0 = production no-op)")

foreach(sym IN ITEMS
        ULMK_CONFIG_MAX_THREADS
        ULMK_CONFIG_MAX_ENDPOINTS
        ULMK_CONFIG_MAX_NOTIFS
        ULMK_CONFIG_MAX_IRQ_BINDINGS)
    if("${${sym}}" LESS 1 OR "${${sym}}" GREATER 256)
        message(FATAL_ERROR "${sym}=${${sym}} out of range [1, 256]")
    endif()
endforeach()
