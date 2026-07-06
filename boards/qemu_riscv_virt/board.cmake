# SPDX-License-Identifier: MIT
#
# Board descriptor for QEMU rv32 virt platform.
# boards/qemu_riscv_virt/board.cmake

set(UL_BOARD_ARCH "riscv")
set(ULMK_BOARD_CPU  "rv32imac")

set(ULMK_BOARD_CFLAGS
    -DULMK_ARCH_HAVE_FPU=0
    -DULMK_ARCH_PMP_NUM=8
    -DULMK_ARCH_IDLE_IS_WFI=0
    -DULMK_ARCH_HAVE_CLINT=0
    -DULMK_ARCH_HAVE_CLIC=0
    -DULMK_ARCH_HAVE_PLIC=1
    -DULMK_BOARD_PLIC_BASE=0x0C000000u
)

if(DEFINED CMAKE_C_FLAGS)
    string(APPEND CMAKE_C_FLAGS " -march=rv32imac_zicsr_zifencei -mabi=ilp32")
    string(APPEND CMAKE_ASM_FLAGS " -march=rv32imac_zicsr_zifencei -mabi=ilp32")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -march=rv32imac_zicsr_zifencei -mabi=ilp32")
endif()

set(ULMK_BOARD_SOURCES
    qemu_console.c
    board_console.c
    board_timer.c
    board_services.c
)

set(UL_BOARD_QEMU_MACHINE "virt")
set(UL_BOARD_QEMU_CPU "rv32")
set(UL_BOARD_QEMU_EXTRA "-bios" "none" "-m" "16M")
