# =============================================================================
# ulmk — multi-arch development environment (TriCore + RISC-V)
#
# Two stages:
#   toolchain  — tricore-elf-gcc + riscv-none-elf-gcc + CMake + Ninja
#   dev        — QEMU TriCore (linumiz) + QEMU RISC-V (upstream apt)
#
# Build manually:
#   docker build --target dev -t ulipe-microkernel:dev tools/docker/
#
# Or use the helper (builds automatically if image is absent):
#   python3 tools/dev.py
# =============================================================================

# -----------------------------------------------------------------------------
# Stage 1 — toolchain: tricore-elf-gcc + tricore-elf-gdb + build tools
#
# Uses the NoMore201/tricore-gcc-toolchain pre-built release:
#   https://github.com/NoMore201/tricore-gcc-toolchain/releases
# Includes GCC 13.4, Binutils 2.40, GDB 14.0 — all with TriCore support.
# -----------------------------------------------------------------------------
FROM ubuntu:24.04 AS toolchain

LABEL description="ulmk TriCore + RISC-V toolchains"
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        wget \
        curl \
        python3 \
        python3-pip \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG TC_TOOLCHAIN_VERSION=13.4.1
ARG TC_TOOLCHAIN_ARCHIVE=tricore-gcc-${TC_TOOLCHAIN_VERSION}-linux.tar.gz

RUN set -e && \
    URL="https://github.com/NoMore201/tricore-gcc-toolchain/releases/download/${TC_TOOLCHAIN_VERSION}/${TC_TOOLCHAIN_ARCHIVE}" && \
    wget -q "${URL}" -O /tmp/tricore-gcc.tar.gz && \
    tar -xf /tmp/tricore-gcc.tar.gz -C /opt && \
    rm /tmp/tricore-gcc.tar.gz && \
    GCC_BIN=$(find /opt -name "tricore-elf-gcc" -type f | head -1) && \
    GCC_BINDIR=$(dirname "${GCC_BIN}") && \
    ln -sfn "${GCC_BINDIR}" /opt/tricore-gcc-bin && \
    echo "GCC: $("${GCC_BIN}" --version | head -1)" && \
    echo "GDB: $(${GCC_BINDIR}/tricore-elf-gdb --version 2>/dev/null | head -1 || echo not found)"

ENV PATH="/opt/tricore-gcc-bin:${PATH}"
RUN tricore-elf-gcc --version
RUN tricore-elf-gdb --version

# xPack RISC-V embedded GCC (riscv-none-elf)
ARG RISCV_XPACK_VERSION=14.2.0-3
ARG RISCV_XPACK_ARCHIVE=xpack-riscv-none-elf-gcc-${RISCV_XPACK_VERSION}-linux-x64.tar.gz

RUN set -e && \
    URL="https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v${RISCV_XPACK_VERSION}/${RISCV_XPACK_ARCHIVE}" && \
    wget -q "${URL}" -O /tmp/riscv-gcc.tar.gz && \
    tar -xf /tmp/riscv-gcc.tar.gz -C /opt && \
    rm /tmp/riscv-gcc.tar.gz && \
    GCC_BIN=$(find /opt -name "riscv-none-elf-gcc" -type f | head -1) && \
    GCC_BINDIR=$(dirname "${GCC_BIN}") && \
    ln -sfn "${GCC_BINDIR}" /opt/riscv-gcc-bin && \
    echo "RISC-V GCC: $("${GCC_BIN}" --version | head -1)"

ENV PATH="/opt/riscv-gcc-bin:${PATH}"
RUN riscv-none-elf-gcc --version

WORKDIR /workspace

# -----------------------------------------------------------------------------
# Stage 2 — dev: adds QEMU TC27x (linumiz fork) with full interrupt support
#
# The linumiz fork adds:
#   - KIT_AURIX_TC277_TRB machine with TC27x peripherals
#   - Interrupt router (IR/SRC) emulation
#   - VIRT debug device at 0xBF000000 (putchar/exit for tests)
#
# PSW patch: QEMU's TriCore resets in user mode (IO=0); real TC1.6.1 hw
# resets in supervisor mode (IO=2).  The sed patch below fixes this so
# CSFRs (FCX, LCX, BIV, BTV, ISP) can be written from _start.
# -----------------------------------------------------------------------------
FROM toolchain AS dev

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        qemu-system-misc \
        libglib2.0-dev \
        libfdt-dev \
        libpixman-1-dev \
        zlib1g-dev \
        libslirp-dev \
        meson \
        pkg-config \
        flex \
        bison \
        libssl-dev \
        ninja-build \
        python3-tomli \
    && rm -rf /var/lib/apt/lists/*

ARG QEMU_REPO=https://gitlab.com/linumiz/infineon/release/qemu-aurix.git
ARG QEMU_BRANCH=release/ifx/tricore-2.0

RUN set -e && \
    git clone --depth=1 --branch "${QEMU_BRANCH}" "${QEMU_REPO}" /tmp/qemu-src && \
    sed -i \
        's/env->PSW |= MASK_PSW_IE;/env->PSW |= MASK_PSW_IE;\n    \/* TC1.6.1 HW resets in supervisor mode; QEMU defaults to user mode. *\/\n    env->PSW = (env->PSW \& ~MASK_PSW_IO) | (TRICORE_PRIV_SM << 10);/' \
        /tmp/qemu-src/target/tricore/cpu.c && \
    mkdir /tmp/qemu-src/build && \
    cd /tmp/qemu-src/build && \
    ../configure \
        --target-list=tricore-softmmu \
        --prefix=/opt/qemu-tricore \
        --disable-docs \
        --disable-debug-info && \
    make -j"$(nproc)" && \
    make install && \
    rm -rf /tmp/qemu-src

ENV PATH="/opt/qemu-tricore/bin:${PATH}"
RUN qemu-system-tricore --version
RUN qemu-system-riscv32 --version

RUN tricore-elf-gdb --version && echo "GDB OK"

# -----------------------------------------------------------------------------
# Unity — lightweight C unit testing framework for embedded targets
#   https://github.com/ThrowTheSwitch/Unity
# Installed to /opt/unity/{unity.c,unity.h,unity_internals.h}
# Makefile in tests/ references UNITY_DIR=/opt/unity
# -----------------------------------------------------------------------------
ARG UNITY_VERSION=v2.6.1
RUN set -e && \
    mkdir -p /opt/unity && \
    BASE="https://raw.githubusercontent.com/ThrowTheSwitch/Unity/${UNITY_VERSION}/src" && \
    wget -q "${BASE}/unity.c"            -O /opt/unity/unity.c && \
    wget -q "${BASE}/unity.h"            -O /opt/unity/unity.h && \
    wget -q "${BASE}/unity_internals.h"  -O /opt/unity/unity_internals.h && \
    echo "Unity ${UNITY_VERSION} installed at /opt/unity"

WORKDIR /workspace
CMD ["/bin/bash"]
