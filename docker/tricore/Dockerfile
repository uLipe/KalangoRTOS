# =============================================================================
# KalangoRTOS — TriCore build & test environment
#
# Two build targets:
#   builder  — installs GCC + builds firmware (fast, ~5 min)
#   tester   — adds linumiz QEMU fork with TC27x interrupt support (slow, ~30 min)
#
# Build (compiler validation only):
#   docker build --target builder -t kalango-tricore:build docker/tricore/
#   docker run --rm -v $(pwd):/src kalango-tricore:build /src/docker/tricore/run_build.sh
#
# Build + test (full QEMU run):
#   docker build --target tester -t kalango-tricore:test docker/tricore/
#   docker run --rm -v $(pwd):/src kalango-tricore:test /src/docker/tricore/run_tests.sh
# =============================================================================

# ---------------------------------------------------------------------------
# Stage 1 — builder: tricore-elf-gcc + CMake
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS builder

LABEL description="KalangoRTOS TriCore build environment"
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        wget \
        curl \
        unzip \
        python3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Linumiz AURIX GCC toolchain
# Released at: https://github.com/Linumiz/aurix-gcc-toolchain/releases
# Naming convention: aurixgcc_<month>-<year>_Linux_x86-x64.zip
ARG GCC_VERSION=11.3.0
ARG GCC_ARCHIVE=aurixgcc_03-2026_Linux_x86-x64

RUN set -e && \
    URL="https://github.com/Linumiz/aurix-gcc-toolchain/releases/download/v${GCC_VERSION}/${GCC_ARCHIVE}.zip" && \
    wget -q "${URL}" -O /tmp/aurixgcc.zip && \
    unzip -q /tmp/aurixgcc.zip -d /opt/aurixgcc-extract && \
    rm /tmp/aurixgcc.zip && \
    find /opt/aurixgcc-extract -type f | xargs chmod +x 2>/dev/null || true && \
    GCC_GCC=$(find /opt/aurixgcc-extract -name "tricore-elf-gcc" -type f | head -1) && \
    GCC_BINDIR=$(dirname "${GCC_GCC}") && \
    ln -sfn "${GCC_BINDIR}" /opt/tricore-gcc-bin && \
    echo "Installed: $("${GCC_GCC}" --version | head -1)"

ENV PATH="/opt/tricore-gcc-bin:${PATH}"

RUN tricore-elf-gcc --version

WORKDIR /src

# ---------------------------------------------------------------------------
# Stage 2 — tester: adds QEMU with TC27x interrupt support
#
# Uses the linumiz QEMU fork which adds full AURIX TC27x/TC29x peripheral
# and interrupt emulation on top of upstream QEMU.
# ---------------------------------------------------------------------------
FROM builder AS tester

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
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
        python3-pip \
        ninja-build \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir tomli

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

WORKDIR /src
