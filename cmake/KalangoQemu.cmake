include(${CMAKE_CURRENT_LIST_DIR}/KalangoSources.cmake)

# ---------------------------------------------------------------------------
# kalango_add_qemu_firmware — ARM Cortex-M QEMU firmware target
# ---------------------------------------------------------------------------
function(kalango_add_qemu_firmware target)
    set(options "")
    set(oneValueArgs CPU FPU_ABI FPU_TYPE CONFIG_HEADER LINKER_SCRIPT STARTUP MACHINE)
    set(multiValueArgs EXTRA_SOURCES)
    cmake_parse_arguments(QEMU "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT QEMU_CPU OR NOT QEMU_CONFIG_HEADER OR NOT QEMU_LINKER_SCRIPT OR NOT QEMU_STARTUP)
        message(FATAL_ERROR "kalango_add_qemu_firmware: missing required arguments")
    endif()

    set(_sources
        ${KALANGO_KERNEL_SOURCES}
        ${KALANGO_QEMU_PLATFORM_SOURCES}
        ${QEMU_STARTUP}
        ${QEMU_EXTRA_SOURCES}
    )

    add_executable(${target} ${_sources})

    set(_cpu_flags -mcpu=${QEMU_CPU} -mthumb)
    if(QEMU_FPU_ABI AND QEMU_FPU_TYPE)
        list(APPEND _cpu_flags -mfloat-abi=${QEMU_FPU_ABI} -mfpu=${QEMU_FPU_TYPE})
    endif()

    target_compile_options(${target} PRIVATE
        ${_cpu_flags}
        -Wall -Werror
        -Os -g
        -ffunction-sections -fdata-sections
        -include "${QEMU_CONFIG_HEADER}"
        -DKALANGO_QEMU=1
    )

    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:${_cpu_flags}>
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/platforms/qemu/include
        ${CMAKE_SOURCE_DIR}/source/utils
    )

    target_link_options(${target} PRIVATE
        ${_cpu_flags}
        -T "${QEMU_LINKER_SCRIPT}"
        -nostartfiles
        -specs=nosys.specs
        -Wl,--gc-sections
    )

    target_link_libraries(${target} PRIVATE gcc c nosys)

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${target}.elf"
        SUFFIX ""
    )

    if(QEMU_MACHINE)
        set_target_properties(${target} PROPERTIES KALANGO_QEMU_MACHINE "${QEMU_MACHINE}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# kalango_add_tricore_firmware — TriCore TC1.6.x / AURIX QEMU firmware target
#
# Usage:
#   kalango_add_tricore_firmware(<target>
#       CONFIG_HEADER  <path/to/kalango_config_tricore.h>
#       [CPU           <tc27xx|tc29xx|...>]      default: tc27xx
#       [LINKER_SCRIPT <path/to/linker.ld>]
#       [STARTUP       <path/to/startup.S>]
#       [EXTRA_SOURCES <src1> ...]
#   )
# ---------------------------------------------------------------------------
function(kalango_add_tricore_firmware target)
    set(options "")
    set(oneValueArgs CPU CONFIG_HEADER LINKER_SCRIPT STARTUP)
    set(multiValueArgs EXTRA_SOURCES)
    cmake_parse_arguments(TC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TC_CONFIG_HEADER)
        message(FATAL_ERROR "kalango_add_tricore_firmware: CONFIG_HEADER is required")
    endif()

    if(NOT TC_CPU)
        set(TC_CPU "tc27xx")
    endif()

    if(NOT TC_LINKER_SCRIPT)
        set(TC_LINKER_SCRIPT
            "${CMAKE_SOURCE_DIR}/platforms/qemu/tricore/linker.ld")
    endif()

    if(NOT TC_STARTUP)
        set(TC_STARTUP
            "${CMAKE_SOURCE_DIR}/platforms/qemu/tricore/startup.S")
    endif()

    set(_sources
        ${KALANGO_COMMON_SOURCES}
        ${KALANGO_ARCH_TRICORE_SOURCES}
        "${CMAKE_SOURCE_DIR}/platforms/qemu/tricore/platform.c"
        "${TC_STARTUP}"
        ${TC_EXTRA_SOURCES}
    )

    add_executable(${target} ${_sources})

    target_compile_options(${target} PRIVATE
        -mcpu=${TC_CPU}
        -Wall -Werror
        -Os -g
        -ffunction-sections -fdata-sections
        -ffreestanding
        -include "${TC_CONFIG_HEADER}"
    )

    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:
            -mcpu=${TC_CPU}
            -x assembler-with-cpp
            -include "${TC_CONFIG_HEADER}"
        >
    )

    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/source/archs"
        "${CMAKE_SOURCE_DIR}/platforms/qemu/include"
    )

    target_link_options(${target} PRIVATE
        -mcpu=${TC_CPU}
        -T "${TC_LINKER_SCRIPT}"
        -nostartfiles
        -Wl,--gc-sections
    )

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${target}.elf"
        SUFFIX ""
    )
endfunction()
