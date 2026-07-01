cmake_minimum_required(VERSION 3.20)

# Must set toolchain file BEFORE project() so that CMake detects the cross compiler.
# cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-tricore-gcc.cmake -S . -B build

project(ulipeMicroKernel
    VERSION 0.1.0
    DESCRIPTION "Minimal capability-based microkernel for TriCore TC2xx"
    LANGUAGES C ASM)

# ---------------------------------------------------------------------------
# Build configuration
# ---------------------------------------------------------------------------

include(cmake/config.cmake)

configure_file(
    cmake/config.h.in
    "${CMAKE_BINARY_DIR}/generated/ul/config.h"
    @ONLY)

# ---------------------------------------------------------------------------
# Linker API (ul_add_app, ul_add_domain, ul_set_root_thread)
# ---------------------------------------------------------------------------

include(cmake/linker_api.cmake)

# ---------------------------------------------------------------------------
# Chip / board selection
# Default to the built-in QEMU platform for CI.
# Override with: cmake -DUL_CHIP_DIR=<path/to/board> ...
# ---------------------------------------------------------------------------

set(UL_CHIP_DIR "${CMAKE_SOURCE_DIR}/boards/qemu_tc27x"
    CACHE PATH "Path to the board directory containing memory.ld")

# ---------------------------------------------------------------------------
# Kernel static library
# ---------------------------------------------------------------------------

add_library(ulipe_kernel STATIC
    kernel/kernel_main.c
    kernel/syscall/syscall_router.c
    kernel/printk/ul_printk.c
    kernel/thread/thread.c
    kernel/sched/sched.c
    kernel/ipc/ep.c
    kernel/notif/notif.c
    kernel/mem/mem.c
    kernel/mem/tlsf.c
    kernel/irq/irq.c
    arch/tricore/arch.c
    arch/tricore/ctx_switch.S
    stub/board_init_stub.c
    stub/printk_stub.c
    stub/root_thread_stub.c
    boards/qemu_tc27x/qemu_console.c)

target_include_directories(ulipe_kernel PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/arch/tricore/include"
    "${CMAKE_BINARY_DIR}/generated")

target_include_directories(ulipe_kernel PRIVATE
    "${CMAKE_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Kernel executable (kernel + arch startup + apps)
# ---------------------------------------------------------------------------

add_executable(ulipe_microkernel
    arch/tricore/startup.S
    arch/tricore/vectors.S)

target_link_libraries(ulipe_microkernel PRIVATE ulipe_kernel)

# ---------------------------------------------------------------------------
# App discovery — sibling directory (docs/build_system_spec.md §4)
# ---------------------------------------------------------------------------

set(APPS_DIR "${CMAKE_SOURCE_DIR}/../ulipeMicroKernel_apps")

if(IS_DIRECTORY "${APPS_DIR}")
    message(STATUS "Discovered apps directory: ${APPS_DIR}")
    add_subdirectory("${APPS_DIR}" "${CMAKE_BINARY_DIR}/apps")
else()
    message(STATUS "No apps directory found; linking root_thread_stub only")
endif()

# ---------------------------------------------------------------------------
# Linker script generation — must be called AFTER all ul_add_app/domain calls
# ---------------------------------------------------------------------------

_ul_finalize_build(ulipe_microkernel "${UL_CHIP_DIR}")

# ---------------------------------------------------------------------------
# Post-build: print memory usage
# ---------------------------------------------------------------------------

add_custom_command(TARGET ulipe_microkernel POST_BUILD
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:ulipe_microkernel>"
    COMMENT "Memory usage")
