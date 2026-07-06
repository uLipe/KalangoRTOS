cmake_minimum_required(VERSION 3.20)

# Toolchain + board/arch resolution: pass -DCMAKE_TOOLCHAIN_FILE and
# -DULMK_CHIP_DIR on the cmake command line (see tools/dev.py).

project(ulmk
    VERSION 0.1.0
    DESCRIPTION "Minimal capability-based microkernel"
    LANGUAGES C ASM)

# Re-sync arch from ULMK_CHIP_DIR (FORCE overwrites stale cache entries).
include(cmake/board_resolve.cmake)

include(cmake/arch_sources.cmake)

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
# Chip / board selection (board.cmake already included by board_resolve.cmake)
# ---------------------------------------------------------------------------

if(NOT DEFINED ULMK_CHIP_DIR)
    set(ULMK_CHIP_DIR "${CMAKE_SOURCE_DIR}/boards/qemu_tc3xx"
        CACHE PATH "Path to the board directory containing memory.ld")
    get_filename_component(ULMK_CHIP_DIR "${ULMK_CHIP_DIR}" ABSOLUTE)
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
    kernel/ipc/ep.c
    kernel/notif/notif.c
    kernel/mem/mem.c
    kernel/mem/tlsf.c
    kernel/irq/irq.c
    ${ULMK_ARCH_KERNEL_SOURCES})

if(EXISTS "${ULMK_CHIP_DIR}/qemu_printk_hook.c")
    target_sources(ulmk_kernel PRIVATE "${ULMK_CHIP_DIR}/qemu_printk_hook.c")
endif()

target_include_directories(ulmk_kernel PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${ULMK_ARCH_DIR}/include"
    "${CMAKE_BINARY_DIR}/generated")

target_include_directories(ulmk_kernel PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${ULMK_CHIP_DIR}")

target_compile_definitions(ulmk_kernel PRIVATE ULMK_KERNEL_BUILD=1)

target_compile_options(ulmk_kernel PRIVATE -fno-data-sections)

if(DEFINED ULMK_BOARD_CFLAGS)
    target_compile_options(ulmk_kernel PRIVATE ${ULMK_BOARD_CFLAGS})
endif()

# ---------------------------------------------------------------------------
# Component scan — kernel/components/ then ../ulmk_apps/ sibling
# ---------------------------------------------------------------------------

message(STATUS "ULMK_ARCH=${ULMK_ARCH}  ULMK_CHIP_DIR=${ULMK_CHIP_DIR}")

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
    ${ULMK_ARCH_EXE_SOURCES}
    ${_ULMK_BOARD_SRCS})

target_include_directories(ulmk PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${ULMK_ARCH_DIR}/include"
    "${ULMK_CHIP_DIR}")

target_compile_options(ulmk PRIVATE -fno-data-sections)

if(DEFINED ULMK_BOARD_CFLAGS)
    target_compile_options(ulmk PRIVATE ${ULMK_BOARD_CFLAGS})
endif()

target_link_libraries(ulmk PRIVATE ulmk_kernel)

_ulmk_finalize_build(ulmk "${ULMK_CHIP_DIR}")

add_custom_command(TARGET ulmk POST_BUILD
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:ulmk>"
    COMMENT "Memory usage")
