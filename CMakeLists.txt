cmake_minimum_required(VERSION 3.5)
project(KalangoRTOS C ASM)

set(KALANGO_CONFIG_FILE_PATH "confs" CACHE STRING "Path to folder that contains the kalango_config.h file")

set(rtos_sources
    "source/archs/arch_armv6m.c"
    "source/archs/arch_armv7m.c"
    "source/archs/arch_c2000.c"
    "source/archs/arch_c2000_asm.S"
    "source/clock.c"
    "source/core.c"
    "source/mutex.c"
    "source/object_pool.c"
    "source/queue.c"
    "source/sched_fifo.c"
    "source/sched_round_robin.c"
    "source/semaphore.c"
    "source/priority_queue.c"
    "source/task.c"
    "source/timer.c"
    "source/utils/print_out.c"
    "source/utils/tlsf.c")

add_library(${PROJECT_NAME} STATIC ${rtos_sources})
target_compile_options(${PROJECT_NAME}  PUBLIC -Wall -Werror -Os -g -ffunction-sections -fdata-sections)
target_include_directories(${PROJECT_NAME} PUBLIC ${KALANGO_CONFIG_FILE_PATH})
target_include_directories(${PROJECT_NAME} PUBLIC  include)
target_include_directories(${PROJECT_NAME} PRIVATE utils)




