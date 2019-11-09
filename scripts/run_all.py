#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build and run all KalangoRTOS QEMU tests")
    parser.add_argument("--build-dir", default=str(BUILD), help="CMake build directory")
    parser.add_argument("--configure", action="store_true", help="Run CMake configure before build")
    parser.add_argument("--examples", action="store_true", help="Also run example firmware")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    build_dir = Path(args.build_dir)

    if args.configure or not build_dir.exists():
        build_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "cmake",
                "-S",
                str(ROOT),
                "-B",
                str(build_dir),
                "-G",
                "Ninja",
                "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake",
                "-DKALANGO_BUILD_TESTS=ON",
                "-DKALANGO_BUILD_EXAMPLES=ON",
            ],
            check=True,
            cwd=ROOT,
        )

    subprocess.run(["cmake", "--build", str(build_dir)], check=True, cwd=ROOT)

    tests = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--output-on-failure", "-R", "kalango_test"],
        cwd=ROOT,
    )

    status = tests.returncode

    if args.examples:
        examples = subprocess.run(
            ["ctest", "--test-dir", str(build_dir), "--output-on-failure", "-R", "example_"],
            cwd=ROOT,
        )
        if examples.returncode != 0:
            status = examples.returncode

    return status


if __name__ == "__main__":
    raise SystemExit(main())
