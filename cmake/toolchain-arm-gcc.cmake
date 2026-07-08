# cmake/toolchain-arm-gcc.cmake — ARM Cortex-M (xpack arm-none-eabi-gcc).
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-gcc.cmake \
#         -DULMK_CHIP_DIR=boards/qemu_mps2_an500 ...
#
# The -mcpu / -mfpu / -mfloat-abi selection is board-specific and appended by
# the board's board.cmake (v7-M vs v8-M, FPU on/off).

include("${CMAKE_CURRENT_LIST_DIR}/board_resolve.cmake")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(_ULMK_ARM_PREFIX "arm-none-eabi"
    CACHE STRING "Cross toolchain prefix")

find_program(CMAKE_C_COMPILER   "${_ULMK_ARM_PREFIX}-gcc"     REQUIRED)
find_program(CMAKE_ASM_COMPILER "${_ULMK_ARM_PREFIX}-gcc"     REQUIRED)
find_program(CMAKE_OBJCOPY      "${_ULMK_ARM_PREFIX}-objcopy" REQUIRED)
find_program(CMAKE_SIZE         "${_ULMK_ARM_PREFIX}-size"    REQUIRED)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# -ffunction-sections/-fdata-sections keep each symbol in its own input section
# so the linker fragments can route kernel code to KERNEL_FLASH and user code to
# the user-text region (MPU isolation); without them plain .text leaks kernel
# code into the user-executable range.
set(CMAKE_C_FLAGS_INIT "-mthumb -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -Wall -Wextra --specs=nosys.specs")
set(CMAKE_ASM_FLAGS_INIT "-mthumb -ffreestanding")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mthumb -nostartfiles -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
