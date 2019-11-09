#!/usr/bin/env python3
"""Run a KalangoRTOS Unity test ELF under QEMU TriCore (linumiz fork)."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

PASS_RE = re.compile(r"(\d+) Tests (\d+) Failures (\d+) Ignored")

TRICORE_MACHINE = "KIT_AURIX_TC277_TRB"


def find_qemu_tricore() -> str:
    qemu = os.environ.get("KALANGO_QEMU_TRICORE", "qemu-system-tricore")
    resolved = shutil.which(qemu)
    if resolved is None:
        raise RuntimeError(
            f"qemu-system-tricore not found. "
            f"Build the linumiz fork or set KALANGO_QEMU_TRICORE env var."
        )
    return resolved


def run_qemu_tricore(
    qemu: str,
    elf: Path,
    timeout: int,
) -> subprocess.CompletedProcess[str]:
    cmd = [
        qemu,
        "-machine", TRICORE_MACHINE,
        "-kernel", str(elf),
        "-nographic",
        "-d", "guest_errors",
    ]
    return subprocess.run(
        cmd,
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a KalangoRTOS Unity test ELF under QEMU TriCore")
    parser.add_argument("--elf", required=True, help="Path to test firmware ELF")
    parser.add_argument("--timeout", type=int, default=120, help="QEMU timeout in seconds")
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

    try:
        result = run_qemu_tricore(qemu, elf, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        print("QEMU timed out", file=sys.stderr)
        return 1

    output = result.stdout + result.stderr
    if output:
        print(output, end="" if output.endswith("\n") else "\n")

    match = PASS_RE.search(output)
    if match:
        failures = int(match.group(2))
        if failures != 0:
            print(f"Unity reported {failures} failures", file=sys.stderr)
            return 1

    if result.returncode != 0:
        print(f"QEMU exited with code {result.returncode}", file=sys.stderr)
        return result.returncode

    print(f"{elf.name}: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
