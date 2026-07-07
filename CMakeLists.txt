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
# Generated config headers — single generator shared with the test Makefiles.
#   generated/ulmk/config.h    kernel sizing/policy (kernel includes this)
#   generated/ulmk/platform.h  SoC snapshot for the arch layer (from board_config.h)
# ---------------------------------------------------------------------------

find_program(ULMK_PYTHON NAMES python3 python REQUIRED)

execute_process(
    COMMAND "${ULMK_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/gen_config.py"
        --out-dir "${CMAKE_BINARY_DIR}/generated"
        --board-config "${ULMK_CHIP_DIR}/board_config.h"
        --set "ULMK_CONFIG_MAX_IRQ_BINDINGS=${ULMK_CONFIG_MAX_IRQ_BINDINGS}"
        --set "ULMK_CONFIG_DEBUG_PRINTK=${ULMK_CONFIG_DEBUG_PRINTK}"
    RESULT_VARIABLE _ulmk_gen_rc)
if(NOT _ulmk_gen_rc EQUAL 0)
    message(FATAL_ERROR "gen_config.py failed (rc=${_ulmk_gen_rc})")
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/gen_config.py"
    "${ULMK_CHIP_DIR}/board_config.h")

# ---------------------------------------------------------------------------
# Kernel static library
# ---------------------------------------------------------------------------

add_library(ulmk_kernel STATIC
    kernel/init/init.c
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

# init.c runs before .data/.bss are initialised — keep GCC from lowering its
# copy/zero loops into memcpy/memset calls.
set_source_files_properties(kernel/init/init.c PROPERTIES
    COMPILE_OPTIONS "-fno-tree-loop-distribute-patterns")

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

# user_entry.c is linked into the image (not libulmk_kernel.a) so it lands in
# the userspace text region, executable by threads at every privilege.
set(_ulmk_exe_sources
    ${ULMK_ARCH_EXE_SOURCES}
    ${_ULMK_BOARD_SRCS}
    ${CMAKE_SOURCE_DIR}/kernel/init/user_entry.c)

if("${_ULMK_ROOT_THREAD_COMP}" STREQUAL "")
    list(APPEND _ulmk_exe_sources "${CMAKE_SOURCE_DIR}/stub/root_thread_stub.c")
endif()

add_executable(ulmk ${_ulmk_exe_sources})

target_include_directories(ulmk PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${ULMK_ARCH_DIR}/include"
    "${CMAKE_BINARY_DIR}/generated"
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
