# KalangoTI.cmake — helper functions for building Kalango with TI toolchains.
#
# Provides:
#   kalango_add_c29_firmware(target sources...)
#   kalango_add_c28x_firmware(target linker_cmd sources...)
#
# Both functions create a static library target with the Kalango kernel sources
# plus the architecture-specific files for the chosen C2000 variant.
#
# Usage (from your application CMakeLists.txt):
#   include(cmake/KalangoTI.cmake)
#   kalango_add_c29_firmware(my_app
#       src/main.c
#       confs/kalango_config_c2000_c29.h  # referenced via -DKALANGO_CONFIG_FILE
#   )

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/source/archs
)

# ----------------------------------------------------------------------------
# C29 firmware target
# ----------------------------------------------------------------------------
function(kalango_add_c29_firmware target)
    set(sources ${ARGN})

    # Core kernel sources (arch guards prevent multi-arch conflicts)
    set(kernel_sources
        ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c29.c
        ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c29_asm.S
        ${CMAKE_SOURCE_DIR}/source/clock.c
        ${CMAKE_SOURCE_DIR}/source/core.c
        ${CMAKE_SOURCE_DIR}/source/irq.c
        ${CMAKE_SOURCE_DIR}/source/mutex.c
        ${CMAKE_SOURCE_DIR}/source/object_pool.c
        ${CMAKE_SOURCE_DIR}/source/queue.c
        ${CMAKE_SOURCE_DIR}/source/sched_fifo.c
        ${CMAKE_SOURCE_DIR}/source/sched_round_robin.c
        ${CMAKE_SOURCE_DIR}/source/semaphore.c
        ${CMAKE_SOURCE_DIR}/source/softirq.c
        ${CMAKE_SOURCE_DIR}/source/task.c
        ${CMAKE_SOURCE_DIR}/source/timer.c
        ${CMAKE_SOURCE_DIR}/source/utils/print_out.c
        ${CMAKE_SOURCE_DIR}/source/utils/tlsf.c
    )

    add_executable(${target} ${sources} ${kernel_sources})

    target_compile_definitions(${target} PRIVATE
        CONFIG_ARCH_C2000=1
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/source/archs
    )
endfunction()


# ----------------------------------------------------------------------------
# C28x firmware target
# ----------------------------------------------------------------------------
function(kalango_add_c28x_firmware target linker_cmd)
    set(sources ${ARGN})

    set(kernel_c_sources
        ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c28.c
        ${CMAKE_SOURCE_DIR}/source/clock.c
        ${CMAKE_SOURCE_DIR}/source/core.c
        ${CMAKE_SOURCE_DIR}/source/irq.c
        ${CMAKE_SOURCE_DIR}/source/mutex.c
        ${CMAKE_SOURCE_DIR}/source/object_pool.c
        ${CMAKE_SOURCE_DIR}/source/queue.c
        ${CMAKE_SOURCE_DIR}/source/sched_fifo.c
        ${CMAKE_SOURCE_DIR}/source/sched_round_robin.c
        ${CMAKE_SOURCE_DIR}/source/semaphore.c
        ${CMAKE_SOURCE_DIR}/source/softirq.c
        ${CMAKE_SOURCE_DIR}/source/task.c
        ${CMAKE_SOURCE_DIR}/source/timer.c
        ${CMAKE_SOURCE_DIR}/source/utils/print_out.c
        ${CMAKE_SOURCE_DIR}/source/utils/tlsf.c
    )

    # The C28x asm file uses TI CG assembler syntax; CMake handles .asm with
    # the ASM_COMPILER set in the toolchain file.
    set(kernel_asm_sources
        ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c28_asm.asm
    )

    add_executable(${target}
        ${sources}
        ${kernel_c_sources}
        ${kernel_asm_sources}
    )

    target_compile_definitions(${target} PRIVATE
        CONFIG_ARCH_C2000_C28=1
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/source/archs
    )

    # Pass linker command file if provided and non-empty
    if(linker_cmd AND EXISTS "${linker_cmd}")
        target_link_options(${target} PRIVATE "${linker_cmd}")
    endif()
endfunction()
