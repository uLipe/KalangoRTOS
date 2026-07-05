# cmake/toolchain-tricore-gcc.cmake
# TriCore ELF toolchain file (NoMore201/tricore-gcc-toolchain in Docker image).
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-tricore-gcc.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR tricore)

set(TRICORE_TRIPLE "tricore-elf")

find_program(CMAKE_C_COMPILER   "${TRICORE_TRIPLE}-gcc"     REQUIRED)
find_program(CMAKE_CXX_COMPILER "${TRICORE_TRIPLE}-g++"     REQUIRED)
find_program(CMAKE_ASM_COMPILER "${TRICORE_TRIPLE}-gcc"     REQUIRED)
find_program(CMAKE_OBJCOPY      "${TRICORE_TRIPLE}-objcopy" REQUIRED)
find_program(CMAKE_SIZE         "${TRICORE_TRIPLE}-size"    REQUIRED)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Freestanding environment: no hosted runtime assumptions.
set(CMAKE_C_FLAGS_INIT   "-ffunction-sections -fdata-sections -ffreestanding")
set(CMAKE_ASM_FLAGS_INIT "")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
