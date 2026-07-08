# SPDX-License-Identifier: MIT
#
# Board descriptor for QEMU mps2-an500 (Cortex-M7, ARMv7-M).
# boards/qemu_mps2_an500/board.cmake
#
# SoC addresses: boards/qemu_mps2_an500/board_config.h

set(UL_BOARD_ARCH "arm")
set(ULMK_BOARD_CPU "cortex-m7")

# Cortex-M7 + single-precision FPU (softfp: FPU used for maths, core-reg ABI).
set(_ULMK_ARM_MFLAGS "-mcpu=cortex-m7 -mfloat-abi=softfp -mfpu=fpv5-sp-d16")
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
)

set(UL_BOARD_QEMU_MACHINE "mps2-an500")
set(UL_BOARD_QEMU_CPU "")
set(UL_BOARD_QEMU_EXTRA "")
