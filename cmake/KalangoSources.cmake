set(KALANGO_COMMON_SOURCES
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

set(KALANGO_ARCH_ARM_SOURCES
    ${CMAKE_SOURCE_DIR}/source/archs/arch_armv7m.c
    ${CMAKE_SOURCE_DIR}/source/archs/arch_armv7m_asm.S
)

# arch_c2000_c28_asm.asm uses TI CG assembler syntax; included only via KalangoTI.cmake
set(KALANGO_ARCH_C2000_SOURCES
    ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c28.c
    ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c29.c
    ${CMAKE_SOURCE_DIR}/source/archs/arch_c2000_c29_asm.S
)

set(KALANGO_ARCH_TRICORE_SOURCES
    ${CMAKE_SOURCE_DIR}/source/archs/arch_tricore.c
    ${CMAKE_SOURCE_DIR}/source/archs/arch_tricore_asm.S
)

# KALANGO_KERNEL_SOURCES — backward-compatible alias for ARM firmware functions.
# Includes ARM + C2000 GCC-compatible arch files. TriCore firmware functions use
# KALANGO_COMMON_SOURCES + KALANGO_ARCH_TRICORE_SOURCES directly.
set(KALANGO_KERNEL_SOURCES
    ${KALANGO_COMMON_SOURCES}
    ${KALANGO_ARCH_ARM_SOURCES}
    ${KALANGO_ARCH_C2000_SOURCES}
    ${KALANGO_ARCH_TRICORE_SOURCES}
)

set(KALANGO_QEMU_PLATFORM_SOURCES
    ${CMAKE_SOURCE_DIR}/platforms/qemu/common/platform.c
    ${CMAKE_SOURCE_DIR}/platforms/qemu/common/semihosting.c
    ${CMAKE_SOURCE_DIR}/platforms/qemu/common/softirq.c
)
