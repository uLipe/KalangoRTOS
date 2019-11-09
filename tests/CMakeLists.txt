cmake_minimum_required(VERSION 3.20)

include(${CMAKE_SOURCE_DIR}/cmake/KalangoQemu.cmake)

set(TEST_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/tests/unity/unity.c
    ${CMAKE_SOURCE_DIR}/tests/framework/test_main.c
    ${CMAKE_SOURCE_DIR}/tests/framework/test_suites.c
    ${CMAKE_SOURCE_DIR}/tests/framework/test_support.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_core.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_tasks.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_semaphore.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_mutex.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_queue.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_timer.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_sched.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_isr.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_edge.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_fpu.c
    ${CMAKE_SOURCE_DIR}/tests/suites/test_softirq.c
)

set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/tests/unity
    ${CMAKE_SOURCE_DIR}/tests/framework
    ${CMAKE_SOURCE_DIR}/tests/suites
)

function(kalango_add_test_target target cpu fpu_abi fpu_type config linker startup machine)
    kalango_add_qemu_firmware(${target}
        CPU ${cpu}
        FPU_ABI ${fpu_abi}
        FPU_TYPE ${fpu_type}
        CONFIG_HEADER ${config}
        LINKER_SCRIPT ${linker}
        STARTUP ${startup}
        MACHINE ${machine}
        EXTRA_SOURCES ${TEST_COMMON_SOURCES}
    )
    target_include_directories(${target} PRIVATE ${TEST_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE UNITY_INCLUDE_CONFIG_H)
endfunction()

# ---------------------------------------------------------------------------
# ARM Cortex-M test targets
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    kalango_add_test_target(
        kalango_test_cortexm3
        cortex-m3 "" ""
        ${CMAKE_SOURCE_DIR}/confs/kalango_config_qemu_cortexm3.h
        ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm3/linker.ld
        ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm3/startup.S
        mps2-an385
    )

    kalango_add_test_target(
        kalango_test_cortexm4f
        cortex-m4 hard fpv4-sp-d16
        ${CMAKE_SOURCE_DIR}/confs/kalango_config_qemu_cortexm4f.h
        ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm4f/linker.ld
        ${CMAKE_SOURCE_DIR}/platforms/qemu/cortexm4f/startup.S
        netduinoplus2
    )

    foreach(test_target IN ITEMS kalango_test_cortexm3 kalango_test_cortexm4f)
        add_test(
            NAME ${test_target}
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_tests.py --elf $<TARGET_FILE:${test_target}>
        )
    endforeach()
endif()

# ---------------------------------------------------------------------------
# TriCore TC27x test target
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "tricore")
    function(kalango_add_tricore_test_target target cpu config)
        kalango_add_tricore_firmware(${target}
            CPU            ${cpu}
            CONFIG_HEADER  ${config}
            EXTRA_SOURCES  ${TEST_COMMON_SOURCES}
        )
        target_include_directories(${target} PRIVATE ${TEST_INCLUDE_DIRS})
        target_compile_definitions(${target} PRIVATE UNITY_INCLUDE_CONFIG_H)
        # Unity is third-party code; disable -Werror for it to avoid noreturn
        # attribute warnings that GCC 11 emits for longjmp-based flow paths.
        set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/tests/unity/unity.c
            TARGET_DIRECTORY ${target}
            PROPERTIES COMPILE_FLAGS "-Wno-error"
        )
    endfunction()

    kalango_add_tricore_test_target(
        kalango_test_tc27x
        tc27xx
        ${CMAKE_SOURCE_DIR}/confs/kalango_config_tricore.h
    )

    add_test(
        NAME kalango_test_tc27x
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_tests_tricore.py
                --elf $<TARGET_FILE:kalango_test_tc27x>
    )
endif()
