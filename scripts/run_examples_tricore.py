#!/usr/bin/env python3
"""Run a KalangoRTOS example ELF under QEMU TriCore (linumiz fork)."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

PASS_MARKERS = ("PASS", "done")
TRICORE_MACHINE = "KIT_AURIX_TC277_TRB"


def find_qemu_tricore() -> str:
    qemu = os.environ.get("KALANGO_QEMU_TRICORE", "qemu-system-tricore")
    resolved = shutil.which(qemu)
    if resolved is None:
        raise RuntimeError(f"qemu-system-tricore not found.")
    return resolved


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a KalangoRTOS example ELF under QEMU TriCore")
    parser.add_argument("--elf", required=True)
    parser.add_argument("--timeout", type=int, default=120)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    elf = Path(args.elf).resolve()

    if not elf.is_file():
        print(f"ELF not found: {elf}", file=sys.stderr)
        return 2

    try:
        qemu = find_qemu_tricore()
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    print(f"Running {elf.name} on {TRICORE_MACHINE} ({qemu})")

    cmd = [
        qemu,
        "-machine", TRICORE_MACHINE,
        "-kernel", str(elf),
        "-nographic",
        "-d", "guest_errors",
    ]

    try:
        result = subprocess.run(
            cmd, check=False, capture_output=True, text=True, timeout=args.timeout
        )
    except subprocess.TimeoutExpired:
        print("QEMU timed out", file=sys.stderr)
        return 1

    output = result.stdout + result.stderr
    if output:
        print(output, end="" if output.endswith("\n") else "\n")

    if result.returncode != 0:
        print(f"QEMU exited with code {result.returncode}", file=sys.stderr)
        return result.returncode

    if not any(m in output for m in PASS_MARKERS):
        print("Missing completion marker", file=sys.stderr)
        return 1

    print(f"{elf.name}: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
