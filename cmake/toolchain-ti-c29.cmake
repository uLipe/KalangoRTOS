# CMake toolchain for TI C29 (c29clang LLVM-based)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-ti-c29.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR c29)

# Locate TI C29 toolchain — prefer env var, fall back to known install path
if(DEFINED ENV{TI_C29_CGT_ROOT})
    set(TI_C29_CGT_ROOT "$ENV{TI_C29_CGT_ROOT}")
else()
    set(TI_C29_CGT_ROOT "/home/ulipe/ti/ccs2040/ccs/tools/compiler/ti-cgt-c29_2.0.0.STS")
endif()

set(CMAKE_C_COMPILER   "${TI_C29_CGT_ROOT}/bin/c29clang")
set(CMAKE_ASM_COMPILER "${TI_C29_CGT_ROOT}/bin/c29clang")
set(CMAKE_AR           "${TI_C29_CGT_ROOT}/bin/c29ar")
set(CMAKE_LINKER       "${TI_C29_CGT_ROOT}/bin/c29lnk")
set(CMAKE_RANLIB       "")

# c29clang accepts standard clang/GCC-style flags
set(CMAKE_C_FLAGS_INIT   "-O2 -mcpu=c29 --target=c29-ti-none-eabi")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=c29 --target=c29-ti-none-eabi")

# Avoid test compile at toolchain detection time (no stdlib available)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Include dirs from the toolchain's runtime
include_directories("${TI_C29_CGT_ROOT}/include")

# SDK F29H85x include paths (optional; needed for device register headers)
if(DEFINED ENV{TI_F29H85X_SDK_ROOT})
    set(TI_F29H85X_SDK_ROOT "$ENV{TI_F29H85X_SDK_ROOT}")
else()
    set(TI_F29H85X_SDK_ROOT "/home/ulipe/ti/f29h85x-sdk_1_02_01_00")
endif()

if(EXISTS "${TI_F29H85X_SDK_ROOT}")
    include_directories(
        "${TI_F29H85X_SDK_ROOT}/source/driverlib/inc"
        "${TI_F29H85X_SDK_ROOT}/source/bitfields"
    )
endif()
