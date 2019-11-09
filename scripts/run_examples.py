#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from kalango_qemu import QemuTarget, find_qemu_system_arm, run_qemu

PASS_MARKERS = ("PASS", "done")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a KalangoRTOS example ELF under QEMU")
    parser.add_argument("--elf", required=True, help="Path to example firmware ELF")
    parser.add_argument("--timeout", type=int, default=120, help="QEMU timeout in seconds")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    elf = Path(args.elf).resolve()

    if not elf.is_file():
        print(f"ELF not found: {elf}", file=sys.stderr)
        return 2

    target = QemuTarget.from_elf_name(elf.name)
    if target is None:
        print(f"Unknown example target for ELF: {elf.name}", file=sys.stderr)
        return 2

    qemu = find_qemu_system_arm()
    print(f"Running {elf.name} on {target.machine} ({qemu})")

    try:
        result = run_qemu(qemu, target, elf, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        print("QEMU timed out", file=sys.stderr)
        return 1

    output = result.stdout + result.stderr
    if output:
        print(output, end="" if output.endswith("\n") else "\n")

    if result.returncode != 0:
        print(f"QEMU exited with code {result.returncode}", file=sys.stderr)
        return result.returncode

    if not any(marker in output for marker in PASS_MARKERS):
        print("Example output missing completion marker", file=sys.stderr)
        return 1

    print(f"{elf.name}: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
