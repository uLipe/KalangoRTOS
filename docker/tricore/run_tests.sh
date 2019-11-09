#!/usr/bin/env bash
# Full build + QEMU test run.
# Run inside the kalango-tricore:test container.
set -euo pipefail

REPO="${1:-/src}"
BUILD="${REPO}/build/tricore"

echo "=== KalangoRTOS TriCore — build & test ==="
echo "GCC:  $(tricore-elf-gcc --version | head -1)"
echo "QEMU: $(qemu-system-tricore --version | head -1)"
echo ""

# Build
cmake -S "${REPO}" \
      -B "${BUILD}" \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="${REPO}/cmake/toolchain-tricore-gcc.cmake" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DKALANGO_BUILD_TESTS=ON \
      -DKALANGO_BUILD_EXAMPLES=ON

cmake --build "${BUILD}" -j"$(nproc)"

echo ""
echo "=== Running tests ==="
cd "${BUILD}"
ctest --output-on-failure --timeout 60

echo ""
echo "=== All tests done ==="
