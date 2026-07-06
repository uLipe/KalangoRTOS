#!/usr/bin/env python3
"""
ulmk development container and build/test driver.

Subcommands:
    (none)              Enter interactive dev container shell
    build               Configure and compile kernel ELF (CMake/Ninja)
    build qemu          Build if needed, then run in QEMU
    components list     Discover components under components/ and ../ulmk_apps/
    components status   Show manifest default vs .ulmk/components.conf
    components enable   Persist component ON in .ulmk/components.conf
    components disable  Remove component from .ulmk/components.conf
    tests unit          Run host-compiled unit tests
    tests integ         Run QEMU integration tests
    killall             Stop all running dev containers

Components default OFF in CMake manifests. Enable before building the demo:

    python3 tools/dev.py components enable hello_world ping_pong
    python3 tools/dev.py build --board boards/qemu_riscv_virt
    python3 tools/dev.py build qemu --board boards/qemu_riscv_virt

One-shot build without saving config:

    python3 tools/dev.py build --component hello_world --component ping_pong
    python3 tools/dev.py build --no-components

Options:
    --rebuild           Force Docker image rebuild (shell entry only)

See also: python3 tools/dev.py --help
          python3 tools/dev.py build --help
          python3 tools/dev.py components --help
          python3 tools/dev.py tests --help
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
COMPONENTS_CONF = WORKSPACE_ROOT / ".ulmk" / "components.conf"

DEFAULT_BOARD  = "boards/qemu_tc3xx"
DEFAULT_BOARD_RISCV = "boards/qemu_riscv_virt"

TEST_TIMEOUT = 120

ARCH_TRICORE_ONLY_TESTS = frozenset({"ctx_early_tricore"})

QEMU_TRICORE = "/opt/qemu-tricore/bin/qemu-system-tricore"
QEMU_RISCV   = "qemu-system-riscv32"


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
# Component discovery and configuration
# ---------------------------------------------------------------------------

_REGISTER_KW = frozenset({
    "NAME", "ENABLED", "SOURCES", "INCLUDE_DIRS", "REQUIRES",
    "ROOT_THREAD", "LINKER_FRAGMENT",
})


def _component_scan_dirs() -> list[Path]:
    """Return directories that may contain ulmk_component_register()."""
    dirs: list[Path] = []

    comp_root = WORKSPACE_ROOT / "components"
    if comp_root.is_dir():
        for entry in sorted(comp_root.iterdir()):
            if not entry.is_dir():
                continue
            if entry.name == "drivers":
                for sub in sorted(entry.iterdir()):
                    if sub.is_dir() and (sub / "CMakeLists.txt").is_file():
                        dirs.append(sub)
            elif (entry / "CMakeLists.txt").is_file():
                dirs.append(entry)

    apps_root = WORKSPACE_ROOT.parent / "ulmk_apps"
    if apps_root.is_dir():
        for entry in sorted(apps_root.iterdir()):
            if entry.is_dir() and (entry / "CMakeLists.txt").is_file():
                dirs.append(entry)

    return dirs


def _parse_component_cmake(path: Path) -> dict | None:
    text = path.read_text()
    m = re.search(r"ulmk_component_register\s*\((.*)\)", text, re.DOTALL)
    if not m:
        return None

    body = m.group(1)
    name_m = re.search(r"NAME\s+(\w+)", body)
    if not name_m:
        return None

    enabled_m = re.search(r"ENABLED\s+(ON|OFF)", body)
    default = enabled_m.group(1) if enabled_m else "OFF"

    requires: list[str] = []
    req_m = re.search(
        r"REQUIRES\s+(.*?)(?=\n\s*(?:"
        + "|".join(_REGISTER_KW - {"REQUIRES"})
        + r")\b|\))",
        body,
        re.DOTALL,
    )
    if req_m:
        requires = req_m.group(1).split()

    return {
        "name": name_m.group(1),
        "default": default,
        "requires": requires,
        "root_thread": bool(re.search(r"\bROOT_THREAD\b", body)),
        "path": path.parent.relative_to(WORKSPACE_ROOT),
    }


def _discover_components() -> list[dict]:
    found: list[dict] = []
    for d in _component_scan_dirs():
        info = _parse_component_cmake(d / "CMakeLists.txt")
        if info:
            found.append(info)
    return sorted(found, key=lambda c: c["name"])


def _component_names() -> list[str]:
    return [c["name"] for c in _discover_components()]


def _load_components_conf() -> set[str]:
    if not COMPONENTS_CONF.is_file():
        return set()
    enabled: set[str] = set()
    for line in COMPONENTS_CONF.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if not line or "=" not in line:
            continue
        name, val = line.split("=", 1)
        name = name.strip()
        val = val.strip().upper()
        if val in ("ON", "1", "YES", "TRUE"):
            enabled.add(name)
    return enabled


def _save_components_conf(enabled: set[str]) -> None:
    COMPONENTS_CONF.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Managed by: python3 tools/dev.py components enable/disable",
        "",
    ]
    for name in sorted(enabled):
        lines.append(f"{name}=ON")
    lines.append("")
    COMPONENTS_CONF.write_text("\n".join(lines))


def _resolve_enabled_components(
    cli_components: list[str] | None,
    no_components: bool,
) -> set[str]:
    if no_components:
        return set(cli_components or [])

    enabled = _load_components_conf()
    for name in cli_components or []:
        enabled.add(name)
    return enabled


def _component_cmake_flags(enabled: set[str]) -> list[str]:
    flags: list[str] = []
    for name in _component_names():
        val = "ON" if name in enabled else "OFF"
        flags.append(f"-DULMK_COMP_{name}_ENABLED={val}")
    return flags


def _validate_component_names(names: list[str]) -> None:
    known = set(_component_names())
    for name in names:
        if name not in known:
            sys.exit(
                f"error: unknown component '{name}'.\n"
                f"Known: {', '.join(sorted(known)) or '(none)'}"
            )


def _run_components(args: argparse.Namespace) -> None:
    action = args.components_action
    comps = _discover_components()

    if action == "list":
        print(f"components ({len(comps)}):")
        for c in comps:
            root = " ROOT_THREAD" if c["root_thread"] else ""
            req = f" REQUIRES={','.join(c['requires'])}" if c["requires"] else ""
            print(f"  {c['name']:20s} default={c['default']}{root}{req}")
            print(f"    {c['path']}")
        return

    conf = _load_components_conf()

    if action == "status":
        print(f"{'COMPONENT':<20} {'DEFAULT':<8} {'CONFIG':<8} {'ROOT':<5} REQUIRES")
        for c in comps:
            cfg = "ON" if c["name"] in conf else "-"
            root = "yes" if c["root_thread"] else "-"
            req = ",".join(c["requires"]) if c["requires"] else "-"
            print(f"{c['name']:<20} {c['default']:<8} {cfg:<8} {root:<5} {req}")
        if conf:
            print(f"\nconfig: {COMPONENTS_CONF.relative_to(WORKSPACE_ROOT)}")
        else:
            print("\nconfig: (none — all components OFF)")
        return

    if action == "enable":
        _validate_component_names(args.names)
        conf |= set(args.names)
        _save_components_conf(conf)
        print(f"enabled: {', '.join(args.names)}")
        print(f"saved → {COMPONENTS_CONF.relative_to(WORKSPACE_ROOT)}")
        return

    if action == "disable":
        _validate_component_names(args.names)
        for name in args.names:
            conf.discard(name)
        _save_components_conf(conf)
        print(f"disabled: {', '.join(args.names)}")
        print(f"saved → {COMPONENTS_CONF.relative_to(WORKSPACE_ROOT)}")
        return

    sys.exit(f"error: unknown components action '{action}'")


# ---------------------------------------------------------------------------
# Build helpers
# ---------------------------------------------------------------------------

def _parse_board_cmake(board_dir: Path) -> dict:
    """Extract UL_BOARD_* variables from board.cmake."""
    cmake_file = board_dir / "board.cmake"
    if not cmake_file.exists():
        sys.exit(f"error: board.cmake not found in {board_dir}")

    text = cmake_file.read_text()
    board: dict = {}
    for m in re.finditer(r'set\(\s*(\w+)\s+(.*?)\)', text, re.DOTALL):
        var = m.group(1)
        raw = re.sub(r'#[^\n]*', '', m.group(2))
        values = [v.strip('"') for v in raw.split()]
        board[var] = values[0] if len(values) == 1 else values
    return board


def _board_arch(board: dict) -> str:
    arch = board.get("UL_BOARD_ARCH", "tricore")
    if isinstance(arch, list):
        arch = arch[0] if arch else "tricore"
    return arch


def _toolchain_for_arch(arch: str) -> str:
    return f"/workspace/cmake/toolchain-{arch}-gcc.cmake"


def _qemu_binary(arch: str) -> str:
    if arch == "riscv":
        return QEMU_RISCV
    return QEMU_TRICORE


def _resolve_board_path(board_arg: str | None) -> tuple[Path, str, list[str]]:
    if board_arg:
        board_host = Path(board_arg).resolve()
        if not board_host.is_dir():
            sys.exit(f"error: board directory not found: {board_arg}")
        if board_host.is_relative_to(WORKSPACE_ROOT):
            board_container = f"/workspace/{board_host.relative_to(WORKSPACE_ROOT)}"
            extra_mounts: list[str] = []
        else:
            board_container = "/board"
            extra_mounts = ["--volume", f"{board_host}:/board:ro"]
    else:
        board_host = WORKSPACE_ROOT / DEFAULT_BOARD
        board_container = f"/workspace/{DEFAULT_BOARD}"
        extra_mounts = []
    return board_host, board_container, extra_mounts


def _build_shell(board_container: str, board: dict,
                 clean: bool, run_qemu: bool,
                 component_flags: list[str]) -> str:
    arch = _board_arch(board)
    toolchain = _toolchain_for_arch(arch)
    build_subdir = f"ulipe-{arch}"
    qemu = _qemu_binary(arch)

    lines: list[str] = [
        "set -e",
        f'export PATH="/opt/qemu-tricore/bin:/opt/tricore-gcc-bin:'
        f'/opt/riscv-gcc-bin:${{PATH}}"',
        "",
    ]

    if clean:
        lines += [
            "echo '--- clean ---'",
            f"rm -rf /build/{build_subdir}",
            "",
        ]

    cfg = [
        "echo '--- configure ---'",
        f"cmake -S /workspace -B /build/{build_subdir} \\",
        f"    -DCMAKE_TOOLCHAIN_FILE={toolchain} \\",
        f"    -DULMK_CHIP_DIR={board_container} \\",
    ]
    for flag in component_flags:
        cfg.append(f"    {flag} \\")
    cfg += [
        "    -GNinja \\",
        "    --no-warn-unused-cli",
        "",
    ]
    lines.extend(cfg)

    lines += [
        "echo '--- build ---'",
        f"ninja -C /build/{build_subdir}",
        "",
        f"echo 'Build OK → /build/{build_subdir}/ulmk'",
    ]

    if run_qemu:
        machine = board.get("UL_BOARD_QEMU_MACHINE", "")
        if isinstance(machine, list):
            machine = machine[0] if machine else ""
        extra = board.get("UL_BOARD_QEMU_EXTRA", "")
        if isinstance(extra, list):
            extra_args = " ".join(extra)
        else:
            extra_args = str(extra) if extra else ""
        lines += [
            "",
            "echo '--- running in QEMU (Ctrl-C to stop) ---'",
            f"{qemu} -machine {machine} {extra_args} \\",
            f"    -kernel /build/{build_subdir}/ulmk -nographic",
        ]

    return "\n".join(lines)


def _qemu_only_shell(board: dict, build_subdir: str) -> str:
    """Run an already-built kernel in QEMU without reconfiguring."""
    arch = _board_arch(board)
    qemu = _qemu_binary(arch)
    machine = board.get("UL_BOARD_QEMU_MACHINE", "")
    if isinstance(machine, list):
        machine = machine[0] if machine else ""
    extra = board.get("UL_BOARD_QEMU_EXTRA", "")
    if isinstance(extra, list):
        extra_args = " ".join(extra)
    else:
        extra_args = str(extra) if extra else ""

    return "\n".join([
        "set -e",
        f'export PATH="/opt/qemu-tricore/bin:/opt/tricore-gcc-bin:'
        f'/opt/riscv-gcc-bin:${{PATH}}"',
        f"test -f /build/{build_subdir}/ulmk",
        "echo '--- running in QEMU (Ctrl-C to stop) ---'",
        f"{qemu} -machine {machine} {extra_args} \\",
        f"    -kernel /build/{build_subdir}/ulmk -nographic",
    ])


def _run_build(args: argparse.Namespace) -> None:
    run_qemu = (args.action == "qemu")

    board_host, board_container, extra_mounts = _resolve_board_path(args.board)
    board = _parse_board_cmake(board_host)
    arch = _board_arch(board)
    build_subdir = f"ulipe-{arch}"

    qemu_machine = board.get("UL_BOARD_QEMU_MACHINE", "")
    if isinstance(qemu_machine, list):
        qemu_machine = qemu_machine[0] if qemu_machine else ""
    if run_qemu and not qemu_machine:
        sys.exit(
            "error: board does not support QEMU "
            "(UL_BOARD_QEMU_MACHINE not set in board.cmake)"
        )

    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    enabled = _resolve_enabled_components(
        getattr(args, "components", None),
        getattr(args, "no_components", False),
    )
    component_flags = _component_cmake_flags(enabled)

    elf = BUILD_DIR / build_subdir / "ulmk"
    if run_qemu and not args.clean and elf.is_file():
        shell_cmd = _qemu_only_shell(board, build_subdir)
    else:
        shell_cmd = _build_shell(
            board_container, board, args.clean, run_qemu, component_flags)

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{WORKSPACE_ROOT}:/workspace",
        "--volume", f"{BUILD_DIR}:/build",
    ] + extra_mounts

    if run_qemu and sys.stdout.isatty():
        cmd += ["--interactive", "--tty"]

    cmd += [IMAGE_NAME, "/bin/bash", "-c", shell_cmd]
    subprocess.run(cmd, check=True)


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
        if d.name.endswith("_e2e"):
            continue
        is_unit = d.name.endswith("_unit")
        if (kind == "unit") == is_unit:
            result.append(d.name)
    return result


def _run_one_shell(kind: str, name: str, arch: str) -> str:
    steps = [
        f"echo '=== {name} (ARCH={arch}) ==='",
        f"cd /workspace/tests/{name}",
        "make clean -s 2>/dev/null || true",
    ]
    if kind == "integ":
        steps.append("make gen_config -s")
    steps.append("rm -rf integ_kernel libtest_kernel.a 2>/dev/null || true")
    steps += [
        f"make -s ARCH={arch}",
        f"timeout --kill-after=5 {TEST_TIMEOUT} make run ARCH={arch}",
    ]
    return " && ".join(steps)


def _filter_integ_tests(tests: list[str], arch: str) -> list[str]:
    if arch == "tricore":
        return tests
    return [t for t in tests if t not in ARCH_TRICORE_ONLY_TESTS]


def _run_all_shell(kind: str, tests: list[str], arch: str) -> str:
    lines = ["FAILED=''"]
    for name in tests:
        snippet = _run_one_shell(kind, name, arch)
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

    board_host, _, _ = _resolve_board_path(getattr(args, "board", None))
    arch = _board_arch(_parse_board_cmake(board_host))

    if args.list:
        tests = _discover_tests(kind)
        if kind == "integ":
            tests = _filter_integ_tests(tests, arch)
        print(f"{kind} tests ({len(tests)}) [ARCH={arch}]:")
        for name in tests:
            print(f"  {name}")
        return

    if test_name:
        available = _discover_tests(kind)
        if kind == "integ":
            available = _filter_integ_tests(available, arch)
        if test_name not in available:
            sys.exit(
                f"error: unknown {kind} test '{test_name}'.\n"
                f"Available: {', '.join(available)}"
            )
        print(f"Running {kind} test: {test_name} (ARCH={arch})")
        shell_cmd = _run_one_shell(kind, test_name, arch)
        cmd = _base_docker_cmd(interactive=False)
        cmd += [IMAGE_NAME, "/bin/bash", "-c", shell_cmd]
        os.execvp("docker", cmd)
    else:
        tests = _discover_tests(kind)
        if kind == "integ":
            tests = _filter_integ_tests(tests, arch)
        if not tests:
            sys.exit(f"No {kind} tests found under {TESTS_DIR}")
        print(f"Running {len(tests)} {kind} test(s) [ARCH={arch}]: {', '.join(tests)}")
        shell_cmd = _run_all_shell(kind, tests, arch)
        cmd = _base_docker_cmd(interactive=False)
        cmd += [IMAGE_NAME, "/bin/bash", "-c", shell_cmd]
        os.execvp("docker", cmd)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def _parse_args() -> argparse.Namespace:
    epilog = """
examples:
  python3 tools/dev.py
  python3 tools/dev.py --rebuild
  python3 tools/dev.py components list
  python3 tools/dev.py components status
  python3 tools/dev.py components enable hello_world ping_pong
  python3 tools/dev.py components disable ping_pong
  python3 tools/dev.py build
  python3 tools/dev.py build --clean
  python3 tools/dev.py build --component hello_world --component ping_pong
  python3 tools/dev.py build --no-components
  python3 tools/dev.py build qemu
  python3 tools/dev.py build --board boards/qemu_riscv_virt
  python3 tools/dev.py build qemu --board boards/qemu_riscv_virt
  python3 tools/dev.py tests unit
  python3 tools/dev.py tests integ
  python3 tools/dev.py tests integ --board boards/qemu_riscv_virt
  python3 tools/dev.py tests integ --test sleep_integ
  python3 tools/dev.py tests integ --list
  python3 tools/dev.py killall
"""
    parser = argparse.ArgumentParser(
        description=(
            "ulmk multi-arch dev container — build, components, tests, QEMU"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="Force a full Docker image rebuild before entering the shell",
    )

    subparsers = parser.add_subparsers(dest="command")

    build_p = subparsers.add_parser(
        "build",
        help="Build the kernel ELF for a target board",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=(
            "Compile the kernel with CMake/Ninja.\n\n"
            "  build       configure + compile\n"
            "  build qemu  compile (if needed) then launch QEMU\n"
            "              uses /opt/qemu-tricore/bin on TriCore and\n"
            "              qemu-system-riscv32 on RISC-V"
        ),
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
        help="Board directory with board.cmake (default: boards/qemu_tc3xx)",
    )
    build_p.add_argument(
        "--clean",
        action="store_true",
        help="Remove previous build artefacts before compiling",
    )

    build_p.add_argument(
        "--component",
        metavar="NAME",
        dest="components",
        action="append",
        default=None,
        help="Enable a component for this build (repeatable; overrides .conf)",
    )
    build_p.add_argument(
        "--no-components",
        action="store_true",
        help="Ignore .ulmk/components.conf; only --component flags apply",
    )

    comp_p = subparsers.add_parser(
        "components",
        help="Discover and enable/disable kernel components",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=(
            "Components live under components/ and ../ulmk_apps/.\n"
            "All are OFF by default in the manifest; enable them explicitly.\n\n"
            "  components list              discover registered components\n"
            "  components status            show manifest vs local config\n"
            "  components enable NAME ...   persist ON in .ulmk/components.conf\n"
            "  components disable NAME ...  remove from local config"
        ),
    )
    comp_sub = comp_p.add_subparsers(dest="components_action", required=True)
    comp_sub.add_parser("list", help="List all discoverable components")
    comp_sub.add_parser("status", help="Show default vs configured state")
    en_p = comp_sub.add_parser("enable", help="Enable components (saved to .conf)")
    en_p.add_argument("names", nargs="+", metavar="NAME")
    dis_p = comp_sub.add_parser("disable", help="Disable components (saved to .conf)")
    dis_p.add_argument("names", nargs="+", metavar="NAME")

    subparsers.add_parser(
        "killall",
        help="Kill all running dev containers (useful when QEMU hangs)",
    )

    tests_p = subparsers.add_parser(
        "tests",
        help="Build and run tests inside the container",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=(
            "Run unit or integration tests inside the dev container.\n\n"
            "  tests unit              host-compiled Unity tests\n"
            "  tests integ             QEMU integration tests\n"
            "  tests integ --list      show available suites\n"
            "  tests integ --test NAME run one suite\n"
            "  tests integ --board PATH  select architecture via board"
        ),
    )
    tests_p.add_argument(
        "kind",
        choices=["unit", "integ"],
        help="Test suite type",
    )
    tests_p.add_argument(
        "--board",
        metavar="PATH",
        default=None,
        help="Board for integ tests (default: boards/qemu_tc3xx)",
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
    elif args.command == "components":
        _run_components(args)
    elif args.command == "tests":
        _run_tests(args)
    else:
        _run_container()


if __name__ == "__main__":
    main()
