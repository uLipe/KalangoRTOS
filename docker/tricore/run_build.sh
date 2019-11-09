#!/usr/bin/env bash
# Build-only validation — no QEMU required.
# Run inside the kalango-tricore:build container.
set -euo pipefail

REPO="${1:-/src}"
BUILD="${REPO}/build/tricore"

echo "=== KalangoRTOS TriCore — compiler validation ==="
echo "GCC: $(tricore-elf-gcc --version | head -1)"
echo ""

cmake -S "${REPO}" \
      -B "${BUILD}" \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="${REPO}/cmake/toolchain-tricore-gcc.cmake" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DKALANGO_BUILD_TESTS=ON \
      -DKALANGO_BUILD_EXAMPLES=ON

cmake --build "${BUILD}" -j"$(nproc)"

echo ""
echo "=== Build successful — ELF files ==="
find "${BUILD}" -name "*.elf" -exec tricore-elf-size {} \;
