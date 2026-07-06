# cmake/toolchain-riscv-gcc.cmake — RISC-V RV32 (xpack riscv-none-elf-gcc).
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv-gcc.cmake \
#         -DULMK_CHIP_DIR=boards/qemu_riscv_virt ...

include("${CMAKE_CURRENT_LIST_DIR}/board_resolve.cmake")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

set(_ULMK_RISCV_PREFIX "riscv-none-elf"
    CACHE STRING "Cross toolchain prefix")

find_program(CMAKE_C_COMPILER   "${_ULMK_RISCV_PREFIX}-gcc"    REQUIRED)
find_program(CMAKE_ASM_COMPILER "${_ULMK_RISCV_PREFIX}-gcc"    REQUIRED)
find_program(CMAKE_OBJCOPY      "${_ULMK_RISCV_PREFIX}-objcopy" REQUIRED)
find_program(CMAKE_SIZE         "${_ULMK_RISCV_PREFIX}-size"    REQUIRED)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -fno-builtin -Wall -Wextra --specs=nosys.specs")
set(CMAKE_ASM_FLAGS_INIT "-ffreestanding")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
