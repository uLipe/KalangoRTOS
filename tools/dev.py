#!/usr/bin/env python3
"""
Launch an interactive development shell inside the ulipeMicroKernel container.

The workspace root is mounted at /workspace inside the container.
Exiting the shell (Ctrl-D or `exit`) stops and removes the container.

Usage:
    python3 tools/dev.py             # build image if missing, then enter
    python3 tools/dev.py --rebuild   # force image rebuild, then enter
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE_NAME = "ulipe-microkernel:dev"
DOCKERFILE_DIR = Path(__file__).parent / "docker"
WORKSPACE_ROOT = Path(__file__).parent.parent.resolve()


def _check_docker() -> None:
    if shutil.which("docker") is None:
        sys.exit("docker not found on PATH — install Docker and try again.")


def _image_exists() -> bool:
    result = subprocess.run(
        ["docker", "image", "inspect", IMAGE_NAME],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return result.returncode == 0


def _build_image() -> None:
    print(f"Building {IMAGE_NAME} …")
    print("(first build compiles QEMU from source — expect ~20-30 min)\n")
    subprocess.run(
        [
            "docker", "build",
            "--target", "dev",
            "-t", IMAGE_NAME,
            str(DOCKERFILE_DIR),
        ],
        check=True,
    )


def _run_container() -> None:
    cmd = [
        "docker", "run",
        "--rm",
        "--interactive",
        "--tty",
        "--volume", f"{WORKSPACE_ROOT}:/workspace",
        "--workdir", "/workspace",
        IMAGE_NAME,
        "/bin/bash",
    ]
    # execvp replaces this process so signals (Ctrl-C) reach docker directly
    # and the parent shell regains control when bash exits.
    os.execvp("docker", cmd)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Launch the ulipeMicroKernel TriCore dev container"
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="Force a full Docker image rebuild before entering",
    )
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    _check_docker()

    if args.rebuild or not _image_exists():
        try:
            _build_image()
        except subprocess.CalledProcessError:
            sys.exit("Docker image build failed.")

    _run_container()


if __name__ == "__main__":
    main()
