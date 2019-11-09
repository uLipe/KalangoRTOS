from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class QemuTarget:
    machine: str
    cpu: str

    @staticmethod
    def from_elf_name(name: str) -> QemuTarget | None:
        if "cortexm3" in name:
            return QemuTarget(machine="lm3s6965evb", cpu="cortex-m3")
        if "cortexm4f" in name:
            return QemuTarget(machine="netduinoplus2", cpu="cortex-m4")
        return None


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def find_qemu_system_arm() -> str:
    qemu = os.environ.get("KALANGO_QEMU", "qemu-system-arm")
    resolved = shutil.which(qemu)
    if resolved is None:
        raise RuntimeError(f"QEMU not found: {qemu}")
    return resolved


def run_qemu(
    qemu: str,
    target: QemuTarget,
    elf: Path,
    timeout: int,
) -> subprocess.CompletedProcess[str]:
    cmd = [
        qemu,
        "-machine",
        target.machine,
        "-cpu",
        target.cpu,
        "-kernel",
        str(elf),
        "-nographic",
        "-semihosting-config",
        "enable=on,target=native",
        "-d",
        "guest_errors",
    ]

    return subprocess.run(
        cmd,
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )
