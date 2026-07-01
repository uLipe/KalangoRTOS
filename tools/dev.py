#!/usr/bin/env python3
"""
Launch an interactive development shell inside the ulipeMicroKernel container.

The workspace root is mounted at /workspace inside the container.
Exiting the shell (Ctrl-D or `exit`) stops and removes the container.

Usage:
    python3 tools/dev.py                       # build image if missing, then enter
    python3 tools/dev.py --rebuild             # force image rebuild, then enter
    python3 tools/dev.py tests unit            # run all unit tests
    python3 tools/dev.py tests integ           # run all integration tests
    python3 tools/dev.py tests unit  --test NAME   # run one unit test
    python3 tools/dev.py tests integ --test NAME   # run one integration test
    python3 tools/dev.py tests unit  --list        # list available unit tests
    python3 tools/dev.py tests integ --list        # list available integ tests
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE_NAME    = "ulipe-microkernel:dev"
DOCKERFILE_DIR = Path(__file__).parent / "docker"
WORKSPACE_ROOT = Path(__file__).parent.parent.resolve()
TESTS_DIR      = WORKSPACE_ROOT / "tests"

TEST_TIMEOUT = 120  # maximum wall-clock seconds for a single `make run`


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
    kind = args.kind
    test_name = args.test

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


def main() -> None:
    args = _parse_args()
    _check_docker()

    if args.rebuild or not _image_exists():
        try:
            _build_image()
        except subprocess.CalledProcessError:
            sys.exit("Docker image build failed.")

    if args.command == "tests":
        _run_tests(args)
    else:
        _run_container()


if __name__ == "__main__":
    main()
