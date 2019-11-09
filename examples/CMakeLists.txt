cmake_minimum_required(VERSION 3.20)

include(${CMAKE_SOURCE_DIR}/cmake/KalangoQemu.cmake)

set(EXAMPLE_COMMON
    ${CMAKE_SOURCE_DIR}/examples/common/mock_peripheral.c
)

set(EXAMPLE_INCLUDE
    ${CMAKE_SOURCE_DIR}/examples/common
)

function(kalango_add_example_target target cpu fpu_abi fpu_type config linker startup machine source)
    kalango_add_qemu_firmware(${target}
        CPU ${cpu}
        FPU_ABI ${fpu_abi}
        FPU_TYPE ${fpu_type}
        CONFIG_HEADER ${config}
        LINKER_SCRIPT ${linker}
        STARTUP ${startup}
        MACHINE ${machine}
        EXTRA_SOURCES ${EXAMPLE_COMMON} ${source}
    )
    target_include_directories(${target} PRIVATE ${EXAMPLE_INCLUDE})
endfunction()

# ---------------------------------------------------------------------------
# ARM Cortex-M example targets
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    set(QEMU_M3_CONFIG ${CMAKE_SOURCE_DIR}/confs/kalango_config_qemu_cortexm3.h)
    set(QEMU_M3_LINKER ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm3/linker.ld)
    set(QEMU_M3_STARTUP ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm3/startup.S)

    macro(kalango_add_example_all_archs name source)
        kalango_add_example_target(example_${name}_cortexm3 cortex-m3 "" "" ${QEMU_M3_CONFIG} ${QEMU_M3_LINKER} ${QEMU_M3_STARTUP} mps2-an385 ${source})
        kalango_add_example_target(example_${name}_cortexm4f cortex-m4 hard fpv4-sp-d16 ${CMAKE_SOURCE_DIR}/confs/kalango_config_qemu_cortexm4f.h ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm4f/linker.ld ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm4f/startup.S netduinoplus2 ${source})
    endmacro()

    kalango_add_example_all_archs(basic_scheduling ${CMAKE_SOURCE_DIR}/examples/basic_scheduling/main.c)
    kalango_add_example_all_archs(producer_consumer ${CMAKE_SOURCE_DIR}/examples/producer_consumer/main.c)
    kalango_add_example_all_archs(button_debounce ${CMAKE_SOURCE_DIR}/examples/button_debounce/main.c)
    kalango_add_example_all_archs(softirq_demo ${CMAKE_SOURCE_DIR}/examples/softirq_demo/main.c)

    foreach(example_target IN ITEMS
            example_basic_scheduling_cortexm3 example_basic_scheduling_cortexm4f
            example_producer_consumer_cortexm3 example_producer_consumer_cortexm4f
            example_button_debounce_cortexm3 example_button_debounce_cortexm4f
            example_softirq_demo_cortexm3 example_softirq_demo_cortexm4f)
        add_test(
            NAME ${example_target}
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_examples.py --elf $<TARGET_FILE:${example_target}>
        )
    endforeach()
endif()

# ---------------------------------------------------------------------------
# TriCore TC27x example targets
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "tricore")
    set(QEMU_TC27X_CONFIG ${CMAKE_SOURCE_DIR}/confs/kalango_config_tricore.h)

    macro(kalango_add_example_tc27x name source)
        kalango_add_tricore_firmware(example_${name}_tc27x
            CPU           tc27xx
            CONFIG_HEADER ${QEMU_TC27X_CONFIG}
            EXTRA_SOURCES ${EXAMPLE_COMMON} ${source}
        )
        target_include_directories(example_${name}_tc27x PRIVATE ${EXAMPLE_INCLUDE})
        add_test(
            NAME example_${name}_tc27x
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_examples_tricore.py
                    --elf $<TARGET_FILE:example_${name}_tc27x>
        )
    endmacro()

    kalango_add_example_tc27x(basic_scheduling
        ${CMAKE_SOURCE_DIR}/examples/basic_scheduling/main.c)
    kalango_add_example_tc27x(producer_consumer
        ${CMAKE_SOURCE_DIR}/examples/producer_consumer/main.c)
    kalango_add_example_tc27x(softirq_demo
        ${CMAKE_SOURCE_DIR}/examples/softirq_demo/main.c)
endif()
