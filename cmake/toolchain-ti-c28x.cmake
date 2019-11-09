# CMake toolchain for TI C28x (cl2000 — TI Code Generation Tools)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-ti-c28x.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR c28x)

# Locate TI C2000 toolchain
if(DEFINED ENV{TI_C2000_CGT_ROOT})
    set(TI_C2000_CGT_ROOT "$ENV{TI_C2000_CGT_ROOT}")
else()
    set(TI_C2000_CGT_ROOT "/home/ulipe/ti/ccs2040/ccs/tools/compiler/ti-cgt-c2000_22.6.3.LTS")
endif()

set(TI_C2000_BIN "${TI_C2000_CGT_ROOT}/bin")

set(CMAKE_C_COMPILER   "${TI_C2000_BIN}/cl2000")
# cl2000 also assembles .asm files; use it as the ASM compiler too
set(CMAKE_ASM_COMPILER "${TI_C2000_BIN}/cl2000")
set(CMAKE_AR           "${TI_C2000_BIN}/ar2000")
set(CMAKE_RANLIB       "")

# cl2000 flags: EABI, C28x core, optimization
# Note: cl2000 uses --compile_only (-c) and --output_file (-fe) like GCC
set(CMAKE_C_FLAGS_INIT
    "--silicon_version=28 --abi=eabi -O2 --opt_for_speed=3 --quiet")
set(CMAKE_ASM_FLAGS_INIT
    "--silicon_version=28 --abi=eabi --quiet")

# cl2000 uses different extensions for object files
set(CMAKE_C_OUTPUT_EXTENSION ".obj")
set(CMAKE_ASM_OUTPUT_EXTENSION ".obj")

# Avoid test compile (no stdlib available in cross-compile context)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Runtime library include paths
include_directories(
    "${TI_C2000_CGT_ROOT}/include"
)

# Runtime library for linking
set(TI_C28X_RUNTIME_LIB "${TI_C2000_CGT_ROOT}/lib/rts2800_eabi.lib")

# Linker command — cl2000 links via lnk2000
set(CMAKE_LINKER "${TI_C2000_BIN}/lnk2000")

# CMake linker flags for cl2000
# The output file flag for cl2000 linker is -o
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--stack_size=0x1000 --heap_size=0x400 --rom_model")
