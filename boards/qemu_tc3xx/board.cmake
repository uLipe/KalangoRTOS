# SPDX-License-Identifier: MIT
#
# Board descriptor for the QEMU AURIX TC3xx emulation target.
# boards/qemu_tc3xx/board.cmake
#
# Consumed by:
#   - tools/dev.py build  (UL_BOARD_* variables parsed via regex)
#   - CMake build         (included directly)
#
# Linker script input: memory.ld in this directory (Layer 3).
# The full linker script is assembled by cmake/generate_ld.py from
# arch and kernel fragments — this board does not provide a complete .ld.

# ── Architecture & CPU ──────────────────────────────────────────────────────
set(UL_BOARD_ARCH "tricore")
set(ULMK_BOARD_CPU  "tc39xx")

# ── Compiler flags ────────────────────────────────────────────────────────────
# Consumed by tools/dev.py build (regex-parsed, no CMake expansion).
# -mcpu is NOT listed here — tools/dev.py derives it from ULMK_BOARD_CPU above.
# For CMake builds the mcpu flag is injected via the block below.
set(ULMK_BOARD_CFLAGS
    -DULMK_ARCH_QEMU_VIRT_CONSOLE=1
    -DULMK_ARCH_SRC_STM0_SR0=0xF0038300u
    -DULMK_ARCH_SRC_SRE_BIT=10u
    -DULMK_ARCH_IDLE_IS_WAIT=0
    -DULMK_ARCH_MPU_NUM_DPR=4
)

# CMake-specific: propagate mcpu to C/ASM compilers and linker.
if(DEFINED CMAKE_C_FLAGS)
    string(APPEND CMAKE_C_FLAGS          " -mcpu=${ULMK_BOARD_CPU}")
    string(APPEND CMAKE_ASM_FLAGS        " -mcpu=${ULMK_BOARD_CPU}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -mcpu=${ULMK_BOARD_CPU}")
endif()

# ── Board sources ─────────────────────────────────────────────────────────────
set(ULMK_BOARD_SOURCES
    qemu_console.c
    board_console.c
    board_timer.c
    board_services.c
)

# ── QEMU emulation ────────────────────────────────────────────────────────────
# Unset or empty means real-hardware-only board (dev.py build qemu will error).
set(UL_BOARD_QEMU_MACHINE "KIT_AURIX_TC397B_TRB")
