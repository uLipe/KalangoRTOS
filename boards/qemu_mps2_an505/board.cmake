# SPDX-License-Identifier: MIT
#
# Board descriptor for QEMU mps2-an505 (Cortex-M33, ARMv8-M mainline).
# boards/qemu_mps2_an505/board.cmake
#
# SoC addresses: boards/qemu_mps2_an505/board_config.h

set(UL_BOARD_ARCH "arm")
set(ULMK_BOARD_CPU "cortex-m33")

# Cortex-M33 + single-precision FPU (softfp: FPU used for maths, core-reg ABI).
set(_ULMK_ARM_MFLAGS "-mcpu=cortex-m33 -mfloat-abi=softfp -mfpu=fpv5-sp-d16")
if(DEFINED CMAKE_C_FLAGS)
    string(APPEND CMAKE_C_FLAGS " ${_ULMK_ARM_MFLAGS}")
    string(APPEND CMAKE_ASM_FLAGS " ${_ULMK_ARM_MFLAGS}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " ${_ULMK_ARM_MFLAGS}")
endif()

set(ULMK_BOARD_SOURCES
    qemu_console.c
    board_console.c
    board_timer.c
    board_services.c
    board_init.c
)

set(UL_BOARD_QEMU_MACHINE "mps2-an505")
set(UL_BOARD_QEMU_CPU "")
set(UL_BOARD_QEMU_EXTRA "")
