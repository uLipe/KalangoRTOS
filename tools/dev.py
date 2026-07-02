#!/usr/bin/env python3
"""
Launch an interactive development shell inside the ulipeMicroKernel container.

The workspace root is mounted at /workspace inside the container.
Exiting the shell (Ctrl-D or `exit`) stops and removes the container.

Usage:
    python3 tools/dev.py                        # build image if missing, then enter
    python3 tools/dev.py --rebuild              # force image rebuild, then enter

    python3 tools/dev.py build                  # build kernel (default QEMU board)
    python3 tools/dev.py build --board PATH     # build with custom board
    python3 tools/dev.py build --clean          # clean rebuild
    python3 tools/dev.py build qemu             # build + run in QEMU

    python3 tools/dev.py tests unit             # run all unit tests
    python3 tools/dev.py tests integ            # run all integration tests
    python3 tools/dev.py tests unit  --test NAME
    python3 tools/dev.py tests integ --test NAME
    python3 tools/dev.py tests unit  --list
    python3 tools/dev.py tests integ --list

    python3 tools/dev.py killall                # kill every running dev container
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE_NAME     = "ulipe-microkernel:dev"
DOCKERFILE_DIR = Path(__file__).parent / "docker"
WORKSPACE_ROOT = Path(__file__).parent.parent.resolve()
TESTS_DIR      = WORKSPACE_ROOT / "tests"
BUILD_DIR      = WORKSPACE_ROOT.parent / "build"

DEFAULT_BOARD  = "boards/qemu_tc3xx"

TEST_TIMEOUT = 120  # maximum wall-clock seconds for a single `make run`


# ---------------------------------------------------------------------------
# Docker helpers
# ---------------------------------------------------------------------------

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


def _base_docker_cmd(interactive: bool) -> list[str]:
    cmd = [
        "docker", "run",
        "--rm",
        "--volume", f"{WORKSPACE_ROOT}:/workspace",
        "--workdir", "/workspace",
    ]
    if interactive:
        cmd += ["--interactive", "--tty"]
    return cmd


def _run_container() -> None:
    cmd = _base_docker_cmd(interactive=True)
    cmd += [IMAGE_NAME, "/bin/bash"]
    os.execvp("docker", cmd)


# ---------------------------------------------------------------------------
# Build helpers
# ---------------------------------------------------------------------------

def _parse_board_cmake(board_dir: Path) -> dict:
    """
    Extract UL_BOARD_* variables from board.cmake.
    Handles single-value and multi-value set() calls (multiline included).
    Returns a dict mapping variable name → str (single) or list[str] (multi).
    Used only to read UL_BOARD_QEMU_MACHINE for the QEMU launch step.
    """
    cmake_file = board_dir / "board.cmake"
    if not cmake_file.exists():
        sys.exit(f"error: board.cmake not found in {board_dir}")

    text = cmake_file.read_text()
    board: dict = {}
    for m in re.finditer(r'set\(\s*(\w+)\s+(.*?)\)', text, re.DOTALL):
        var = m.group(1)
        raw = re.sub(r'#[^\n]*', '', m.group(2))   # strip inline comments
        values = raw.split()
        board[var] = values[0] if len(values) == 1 else values
    return board


def _build_shell(board_container: str, qemu_machine: str,
                 clean: bool, run_qemu: bool) -> str:
    """
    Produce the bash script that runs inside the Docker container.
    Delegates the full compile+link flow to CMake + Ninja.
    """
    lines: list[str] = ["set -e", ""]

    if clean:
        lines += [
            "echo '--- clean ---'",
            "rm -rf /build/ulipe",
            "",
        ]

    lines += [
        "echo '--- configure ---'",
        "cmake -S /workspace -B /build/ulipe \\",
        "    -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/toolchain-tricore-gcc.cmake \\",
        f"    -DUL_CHIP_DIR={board_container} \\",
        "    -GNinja \\",
        "    --no-warn-unused-cli",
        "",
        "echo '--- build ---'",
        "ninja -C /build/ulipe",
        "",
        "echo 'Build OK → /build/ulipe/ulipe_microkernel'",
    ]

    if run_qemu:
        lines += [
            "",
            "echo '--- running in QEMU (Ctrl-C to stop) ---'",
            f"qemu-system-tricore -machine {qemu_machine} \\",
            "    -kernel /build/ulipe/ulipe_microkernel -nographic",
        ]

    return "\n".join(lines)


def _run_build(args: argparse.Namespace) -> None:
    run_qemu = (args.action == "qemu")

    if args.board:
        board_host = Path(args.board).resolve()
        if not board_host.is_dir():
            sys.exit(f"error: board directory not found: {args.board}")
        board_container = "/board"
        extra_mounts: list[str] = ["--volume", f"{board_host}:/board:ro"]
    else:
        board_host      = WORKSPACE_ROOT / DEFAULT_BOARD
        board_container = f"/workspace/{DEFAULT_BOARD}"
        extra_mounts    = []

    board = _parse_board_cmake(board_host)

    qemu_machine = board.get("UL_BOARD_QEMU_MACHINE", "")
    if isinstance(qemu_machine, list):
        qemu_machine = qemu_machine[0] if qemu_machine else ""
    if run_qemu and not qemu_machine:
        sys.exit(
            "error: board does not support QEMU "
            "(UL_BOARD_QEMU_MACHINE not set in board.cmake)"
        )

    cmake_build = BUILD_DIR / "ulipe"
    cmake_build.mkdir(parents=True, exist_ok=True)

    shell_cmd = _build_shell(board_container, qemu_machine, args.clean, run_qemu)

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{WORKSPACE_ROOT}:/workspace",
        "--volume", f"{BUILD_DIR}:/build",
    ] + extra_mounts

    if run_qemu:
        cmd += ["--interactive", "--tty"]

    cmd += [IMAGE_NAME, "/bin/bash", "-c", shell_cmd]
    os.execvp("docker", cmd)


# ---------------------------------------------------------------------------
# Killall
# ---------------------------------------------------------------------------

def _killall() -> None:
    result = subprocess.run(
        ["docker", "ps", "-q", "--filter", f"ancestor={IMAGE_NAME}"],
        capture_output=True,
        text=True,
    )
    containers = result.stdout.split()
    if not containers:
        print("No running containers found.")
        return
    print(f"Killing {len(containers)} container(s)…")
    subprocess.run(["docker", "kill"] + containers, check=False)
    print("Done.")


# ---------------------------------------------------------------------------
# Test runner helpers
# ---------------------------------------------------------------------------

def _discover_tests(kind: str) -> list[str]:
    result = []
    for d in sorted(TESTS_DIR.iterdir()):
        if not d.is_dir() or not (d / "Makefile").exists():
            continue
        is_unit = d.name.endswith("_unit")
        if (kind == "unit") == is_unit:
            result.append(d.name)
    return result


def _has_gen_config(test_name: str) -> bool:
    mf = TESTS_DIR / test_name / "Makefile"
    return "gen_config" in mf.read_text()


def _run_one_shell(kind: str, name: str) -> str:
    steps = [
        f"echo '=== {name} ==='",
        f"cd /workspace/tests/{name}",
        "make clean -s 2>/dev/null || true",
    ]
    if kind == "integ" and _has_gen_config(name):
        steps.append("make gen_config -s")
    steps += [
        "make -s",
        f"timeout --kill-after=5 {TEST_TIMEOUT} make run",
    ]
    return " && ".join(steps)


def _run_all_shell(kind: str, tests: list[str]) -> str:
    lines = ["FAILED=''"]
    for name in tests:
        snippet = _run_one_shell(kind, name)
        lines.append(
            f"if ( {snippet} ); then :; "
            f"else FAILED=\"$FAILED {name}\"; fi"
        )
    lines.append(
        "if [ -z \"$FAILED\" ]; then "
        "echo; echo '=== ALL TESTS PASSED ==='; "
        "else "
        "echo; echo \"=== FAILED:$FAILED ===\"; exit 1; "
        "fi"
    )
    return " ; ".join(lines)


def _run_tests(args: argparse.Namespace) -> None:
    kind      = args.kind
    test_name = args.test

    _killall()

    if args.list:
        tests = _discover_tests(kind)
        print(f"{kind} tests ({len(tests)}):")
        for name in tests:
            print(f"  {name}")
        return

    if test_name:
        available = _discover_tests(kind)
        if test_name not in available:
            sys.exit(
                f"error: unknown {kind} test '{test_name}'.\n"
                f"Available: {', '.join(available)}"
            )
        print(f"Running {kind} test: {test_name}")
        shell_cmd = _run_one_shell(kind, test_name)
        cmd = _base_docker_cmd(interactive=False)
        cmd += [IMAGE_NAME, "/bin/bash", "-c", shell_cmd]
        os.execvp("docker", cmd)
    else:
        tests = _discover_tests(kind)
        if not tests:
            sys.exit(f"No {kind} tests found under {TESTS_DIR}")
        print(f"Running {len(tests)} {kind} test(s): {', '.join(tests)}")
        shell_cmd = _run_all_shell(kind, tests)
        cmd = _base_docker_cmd(interactive=False)
        cmd += [IMAGE_NAME, "/bin/bash", "-c", shell_cmd]
        os.execvp("docker", cmd)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="ulipeMicroKernel TriCore dev container",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="Force a full Docker image rebuild before entering",
    )

    subparsers = parser.add_subparsers(dest="command")

    # ── build ────────────────────────────────────────────────────────────────
    build_p = subparsers.add_parser(
        "build",
        help="Build the kernel ELF for a target board",
    )
    build_p.add_argument(
        "action",
        nargs="?",
        choices=["qemu"],
        default=None,
        help="'qemu' — build (if needed) then run in QEMU",
    )
    build_p.add_argument(
        "--board",
        metavar="PATH",
        default=None,
        help="Path to board directory with board.cmake (default: boards/qemu_tc3xx)",
    )
    build_p.add_argument(
        "--clean",
        action="store_true",
        help="Remove previous build artefacts before compiling",
    )

    # ── killall ──────────────────────────────────────────────────────────────
    subparsers.add_parser(
        "killall",
        help="Kill all running dev containers (useful when QEMU hangs)",
    )

    # ── tests ────────────────────────────────────────────────────────────────
    tests_p = subparsers.add_parser(
        "tests",
        help="Build and run tests inside the container",
    )
    tests_p.add_argument(
        "kind",
        choices=["unit", "integ"],
        help="Test suite type",
    )
    tests_p.add_argument(
        "--test",
        metavar="NAME",
        default=None,
        help="Run a single test by directory name",
    )
    tests_p.add_argument(
        "--list",
        action="store_true",
        help="List available tests of the given kind and exit",
    )

    return parser.parse_args()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    args = _parse_args()
    _check_docker()

    if args.command == "killall":
        _killall()
        return

    if args.rebuild or not _image_exists():
        try:
            _build_image()
        except subprocess.CalledProcessError:
            sys.exit("Docker image build failed.")

    if args.command == "build":
        _run_build(args)
    elif args.command == "tests":
        _run_tests(args)
    else:
        _run_container()


if __name__ == "__main__":
    main()
