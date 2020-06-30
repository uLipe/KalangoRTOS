cmake_minimum_required(VERSION 3.5)
project(KalangoRTOS C ASM)

set(KALANGO_CONFIG_FILE_PATH "confs" CACHE STRING "Path to folder that contains the kalango_config.h file")

set(rtos_sources 
    "archs/arch_armv6m.c"
    "archs/arch_armv7m.c"
    "src/clock.c"
    "src/core.c"
    "src/mutex.c"
    "src/object_pool.c"
    "src/queue.c"
    "src/sched_fifo.c"
    "src/sched_round_robin.c"
    "src/semaphore.c"
    "src/task.c"
    "src/timer.c"
    "utils/print_out.c"
    "utils/tlsf.c")

add_library(${PROJECT_NAME} STATIC ${rtos_sources})
target_compile_options(${PROJECT_NAME}  PUBLIC -Wall -Werror -Os -g -ffunction-sections -fdata-sections -includekalango_config.h)
target_include_directories(${PROJECT_NAME} PUBLIC ${KALANGO_CONFIG_FILE_PATH})
target_include_directories(${PROJECT_NAME} PUBLIC  include)
target_include_directories(${PROJECT_NAME} PRIVATE utils)




