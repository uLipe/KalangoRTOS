cmake_minimum_required(VERSION 3.20)

# Must set toolchain file BEFORE project() so that CMake detects the cross compiler.
# cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-tricore-gcc.cmake -S . -B build

project(ulmk
    VERSION 0.1.0
    DESCRIPTION "Minimal capability-based microkernel for TriCore TC2xx"
    LANGUAGES C ASM)

# ---------------------------------------------------------------------------
# Build configuration
# ---------------------------------------------------------------------------

include(cmake/config.cmake)

configure_file(
    cmake/config.h.in
    "${CMAKE_BINARY_DIR}/generated/ulmk/config.h"
    @ONLY)

# ---------------------------------------------------------------------------
# Linker API (ulmk_add_app, ulmk_add_domain, ulmk_set_root_thread)
# ---------------------------------------------------------------------------

include(cmake/linker_api.cmake)

# ---------------------------------------------------------------------------
# Component API (ulmk_component_register, ulmk_components_finalize)
# ---------------------------------------------------------------------------

include(cmake/component_api.cmake)

# ---------------------------------------------------------------------------
# Chip / board selection
# Default to the built-in QEMU platform for CI.
# Override with: cmake -DULMK_CHIP_DIR=<path/to/board> ...
# ---------------------------------------------------------------------------

set(ULMK_CHIP_DIR "${CMAKE_SOURCE_DIR}/boards/qemu_tc3xx"
    CACHE PATH "Path to the board directory containing memory.ld")

# ---------------------------------------------------------------------------
# Board descriptor — sets ULMK_BOARD_CPU, ULMK_BOARD_CFLAGS, ULMK_BOARD_SOURCES
# ---------------------------------------------------------------------------

if(EXISTS "${ULMK_CHIP_DIR}/board.cmake")
    include("${ULMK_CHIP_DIR}/board.cmake")
endif()

# Prefix board sources with the board directory path.
set(_ULMK_BOARD_SRCS "")
foreach(_src IN LISTS ULMK_BOARD_SOURCES)
    list(APPEND _ULMK_BOARD_SRCS "${ULMK_CHIP_DIR}/${_src}")
endforeach()

# ---------------------------------------------------------------------------
# Kernel static library
# ---------------------------------------------------------------------------

add_library(ulmk_kernel STATIC
    kernel/kernel_main.c
    kernel/syscall/syscall_router.c
    kernel/printk/ulmk_printk.c
    kernel/thread/thread.c
    kernel/sched/sched.c
    kernel/sched/fifo_rt.c
    kernel/sched/bitmap_rt.c
    kernel/timer/timer.c
    kernel/ipc/ep.c
    kernel/notif/notif.c
    kernel/mem/mem.c
    kernel/mem/tlsf.c
    kernel/irq/irq.c
    arch/tricore/arch.c
    arch/tricore/ctx_switch.S
    ${_ULMK_BOARD_SRCS})

target_include_directories(ulmk_kernel PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/arch/tricore/include"
    "${CMAKE_BINARY_DIR}/generated")

target_include_directories(ulmk_kernel PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${ULMK_CHIP_DIR}")

if(DEFINED ULMK_BOARD_CFLAGS)
    target_compile_options(ulmk_kernel PRIVATE ${ULMK_BOARD_CFLAGS})
endif()

# ---------------------------------------------------------------------------
# Component scan — kernel/components/ then ../ulmk_apps/ sibling
# ---------------------------------------------------------------------------

message(STATUS "Scanning components:")

file(GLOB _kernel_comp_dirs LIST_DIRECTORIES true
    "${CMAKE_SOURCE_DIR}/components/*")
foreach(_dir IN LISTS _kernel_comp_dirs)
    if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
        get_filename_component(_cname "${_dir}" NAME)
        add_subdirectory("${_dir}" "${CMAKE_BINARY_DIR}/components/${_cname}")
    endif()
endforeach()

set(_ULMK_APPS_DIR "${CMAKE_SOURCE_DIR}/../ulmk_apps")
if(IS_DIRECTORY "${_ULMK_APPS_DIR}")
    message(STATUS "Scanning ulmk_apps sibling: ${_ULMK_APPS_DIR}")
    file(GLOB _app_comp_dirs LIST_DIRECTORIES true "${_ULMK_APPS_DIR}/*")
    foreach(_dir IN LISTS _app_comp_dirs)
        if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
            get_filename_component(_cname "${_dir}" NAME)
            add_subdirectory("${_dir}" "${CMAKE_BINARY_DIR}/ulmk_apps/${_cname}")
        endif()
    endforeach()
endif()

ulmk_components_finalize()

# ---------------------------------------------------------------------------
# Kernel executable (kernel + arch startup + apps)
# ---------------------------------------------------------------------------

add_executable(ulmk
    arch/tricore/startup.S
    arch/tricore/vectors.S)

target_link_libraries(ulmk PRIVATE ulmk_kernel)

# ---------------------------------------------------------------------------
# Linker script generation — must be called AFTER all ulmk_add_app/domain calls
# ---------------------------------------------------------------------------

_ulmk_finalize_build(ulmk "${ULMK_CHIP_DIR}")

# ---------------------------------------------------------------------------
# Post-build: print memory usage
# ---------------------------------------------------------------------------

add_custom_command(TARGET ulmk POST_BUILD
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:ulmk>"
    COMMENT "Memory usage")
