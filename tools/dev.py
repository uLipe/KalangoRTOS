#!/usr/bin/env python3
"""
Launch an interactive development shell inside the ulipeMicroKernel container.

The workspace root is mounted at /workspace inside the container.
Exiting the shell (Ctrl-D or `exit`) stops and removes the container.

If the Infineon TSIM is installed on the host at the standard path
(/opt/Tools/TSIM-Tricore-instruction-set-simulator), it is bind-mounted
read-only into the container at the same path and TSIM_PATH is exported so
the test Makefiles can find the binary without hard-coding the version.
When TSIM is absent the container falls back to QEMU (always present in the
image).

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

# Standard install location produced by the Infineon TSIM .deb package.
_TSIM_HOST_ROOT = Path("/opt/Tools/TSIM-Tricore-instruction-set-simulator")


def _check_docker() -> None:
    if shutil.which("docker") is None:
        sys.exit("docker not found on PATH — install Docker and try again.")


def _find_tsim() -> tuple[Path | None, Path | None]:
    """
    Locate the TSIM installation on the host.

    Returns (tsim_root, tsim_versioned_dir) where tsim_versioned_dir is the
    first child directory that contains bin/tsim16p_e, or (None, None) if
    TSIM is not found.
    """
    if not _TSIM_HOST_ROOT.is_dir():
        return None, None
    for candidate in sorted(_TSIM_HOST_ROOT.iterdir()):
        binary = candidate / "bin" / "tsim16p_e"
        if binary.is_file() and os.access(binary, os.X_OK):
            return _TSIM_HOST_ROOT, candidate
    return None, None


# Files required inside the container, relative to the versioned TSIM dir.
_TSIM_MIRROR_FILES = [
    "bin/tsim16p_e",
    "config/tc162/tc39xx/MConfig",
    "config/tc162/tc39xx/OConfig",
]


def _ensure_tsim_mirror(tsim_versioned: Path) -> Path | None:
    """
    Make TSIM files accessible from a path that snap Docker can bind-mount.

    snap Docker's AppArmor profile restricts daemon-side bind mounts to paths
    accessible via the 'home' interface (~/).  /opt is not accessible.
    When /opt and $HOME are on the same filesystem (the common case) we create
    hardlinks instead of copies so no extra disk space is used.

    Returns the mirror directory (under ~/.cache) that Docker can mount,
    or None if the mirror cannot be created.
    """
    mirror_root = Path.home() / ".cache" / "ulipe-mkdev" / "tsim"
    mirror_dir = mirror_root / tsim_versioned.name

    same_dev = False
    try:
        same_dev = os.stat(tsim_versioned).st_dev == os.stat(Path.home()).st_dev
    except OSError:
        pass

    for rel in _TSIM_MIRROR_FILES:
        src = tsim_versioned / rel
        dst = mirror_dir / rel

        if not src.exists():
            print(f"  warning: TSIM file not found: {src}", file=sys.stderr)
            return None

        dst.parent.mkdir(parents=True, exist_ok=True)

        # Rebuild the link if the source inode changed (e.g. after TSIM upgrade).
        needs_link = (
            not dst.exists()
            or (same_dev and os.stat(src).st_ino != os.stat(dst).st_ino)
            or (not same_dev and os.stat(src).st_mtime != os.stat(dst).st_mtime)
        )

        if needs_link:
            if dst.exists():
                dst.unlink()
            if same_dev:
                os.link(src, dst)   # zero-copy hardlink
            else:
                import shutil as _shutil
                _shutil.copy2(src, dst)

    return mirror_dir


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
    tsim_root, tsim_versioned = _find_tsim()

    cmd = [
        "docker", "run",
        "--rm",
        "--interactive",
        "--tty",
        "--volume", f"{WORKSPACE_ROOT}:/workspace",
        "--workdir", "/workspace",
    ]

    if tsim_root is not None and tsim_versioned is not None:
        mirror = _ensure_tsim_mirror(tsim_versioned)
        if mirror is not None:
            # Mirror is under ~/.cache — accessible to snap Docker via the
            # 'home' interface.  Present it at the canonical /opt path inside
            # the container so Makefiles don't need to distinguish host vs
            # container paths.
            container_tsim = str(tsim_root / tsim_versioned.name)
            cmd += [
                "--volume", f"{mirror}:{container_tsim}:ro",
                "--env",   f"TSIM_PATH={container_tsim}",
                "--env",   "TSIM_AVAILABLE=1",
            ]
            print(
                f"TSIM found: {tsim_versioned.name}"
                f" — mirror at {mirror}, mounted as {container_tsim}"
            )
        else:
            cmd += ["--env", "TSIM_AVAILABLE=0"]
            print("TSIM mirror could not be created — using QEMU fallback")
    else:
        cmd += ["--env", "TSIM_AVAILABLE=0"]
        print("TSIM not found at /opt/Tools — using QEMU fallback")

    cmd += [IMAGE_NAME, "/bin/bash"]

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
