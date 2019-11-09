set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR tricore)

# Expect the toolchain to be on PATH (installed by Dockerfile)
find_program(TRICORE_GCC tricore-elf-gcc REQUIRED)
get_filename_component(TRICORE_TOOLCHAIN_BIN "${TRICORE_GCC}" DIRECTORY)

set(CMAKE_C_COMPILER   "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-gcc")
set(CMAKE_ASM_COMPILER "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-gcc")
set(CMAKE_AR           "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-ar")
set(CMAKE_RANLIB       "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-ranlib")
set(CMAKE_OBJCOPY      "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-objcopy")
set(CMAKE_OBJDUMP      "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-objdump")
set(CMAKE_SIZE         "${TRICORE_TOOLCHAIN_BIN}/tricore-elf-size")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# TC27xx core (TC1.6.1); switch to -mcpu=tc29xx for AURIX TC3xx
set(TRICORE_CPU "tc27xx" CACHE STRING "TriCore CPU variant")

set(TRICORE_C_FLAGS
    "-mcpu=${TRICORE_CPU}"
    "-ffunction-sections"
    "-fdata-sections"
    "-ffreestanding"
    "-fno-exceptions"
    "-Wall"
    "-Wextra"
)

string(JOIN " " TRICORE_C_FLAGS_STR ${TRICORE_C_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${TRICORE_C_FLAGS_STR}")
set(CMAKE_ASM_FLAGS_INIT "${TRICORE_C_FLAGS_STR} -x assembler-with-cpp")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-mcpu=${TRICORE_CPU} -Wl,--gc-sections -nostartfiles"
)
