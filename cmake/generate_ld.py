#!/usr/bin/env python3
# cmake/generate_ld.py
# Assembles the final linker script from layered fragments.
# Full specification: docs/linker_spec.md §5
#
# Invocation (see linker_api.cmake):
#   generate_ld.py --chip-dir <path> --arch-dir <path> \
#                  --kernel-dir <path> --snippets <path> \
#                  --output <file> \
#                  [--app <name>]... [--domain <name:region>]...
#
# Assembly order:
#   1. arch prologue (arch-dir/prologue.ld.in)
#   2. chip MEMORY block (chip-dir/memory.ld)
#   3. [optional] chip BMHD fragment (chip-dir/bmhd.ld.in, if HAVE_BMHD=1)
#   4. SECTIONS {
#        kernel/vectors.ld.in
#        kernel/kernel_text.ld.in
#        [rendered snippets/app_code.ld.in for each --app]
#        kernel/kernel_data.ld.in
#        kernel/domain_table.ld.in
#        [rendered snippets/domain_data.ld.in for each --domain]
#        [arch/tricore/linker/small_data.ld.in  if HAVE_SMALL_DATA=1]
#        kernel/kernel_stacks.ld.in
#        [arch/tricore/linker/csa_pool.ld.in    if HAVE_CSA=1]
#        kernel/user_pool.ld.in
#      }

import argparse
import os
import re
import sys


def read_fragment(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def render_snippet(path: str, substitutions: dict) -> str:
    content = read_fragment(path)
    for key, val in substitutions.items():
        content = content.replace(f"@{key}@", val)
    return content


def parse_flags(chip_memory_ld: str) -> dict:
    """Extract HAVE_* flag values from the chip memory.ld as integers."""
    flags = {}
    for name in ("HAVE_CSA", "HAVE_SMALL_DATA", "HAVE_BMHD"):
        m = re.search(rf"^\s*{name}\s*=\s*(\d+)\s*;", chip_memory_ld, re.MULTILINE)
        flags[name] = int(m.group(1)) if m else 0
    return flags


def main():
    ap = argparse.ArgumentParser(description="ulmk linker script generator")
    ap.add_argument("--chip-dir",   required=True)
    ap.add_argument("--arch-dir",   required=True)
    ap.add_argument("--kernel-dir", required=True)
    ap.add_argument("--snippets",   required=True)
    ap.add_argument("--output",     required=True)
    ap.add_argument("--app",        action="append", default=[],
                    metavar="NAME")
    ap.add_argument("--domain",     action="append", default=[],
                    metavar="NAME:REGION")
    args = ap.parse_args()

    out = []

    # 1. Arch prologue
    out.append(read_fragment(os.path.join(args.arch_dir, "prologue.ld.in")))

    # 2. Chip MEMORY block
    chip_memory = read_fragment(os.path.join(args.chip_dir, "memory.ld"))
    out.append(chip_memory)

    flags = parse_flags(chip_memory)

    out.append("\nSECTIONS {\n")

    # 3. [optional] BMHD
    bmhd_path = os.path.join(args.chip_dir, "bmhd.ld.in")
    if flags["HAVE_BMHD"] and os.path.exists(bmhd_path):
        out.append(read_fragment(bmhd_path))

    # 4a. Vectors
    out.append(read_fragment(os.path.join(args.kernel_dir, "vectors.ld.in")))

    # 4b. Kernel text
    out.append(read_fragment(os.path.join(args.kernel_dir, "kernel_text.ld.in")))

    # 4c. Per-app code snippets
    app_snippet = os.path.join(args.snippets, "app_code.ld.in")
    for app in args.app:
        out.append(render_snippet(app_snippet, {"APP_NAME": app}))

    # 4d. Kernel data
    out.append(read_fragment(os.path.join(args.kernel_dir, "kernel_data.ld.in")))

    # 4e. Domain table
    out.append(read_fragment(os.path.join(args.kernel_dir, "domain_table.ld.in")))

    # 4f. Per-domain data snippets
    domain_snippet = os.path.join(args.snippets, "domain_data.ld.in")
    for entry in args.domain:
        parts = entry.split(":", 1)
        dname = parts[0]
        dregion = parts[1] if len(parts) > 1 else "KERNEL_RAM"
        out.append(render_snippet(domain_snippet,
                                  {"DOMAIN_NAME": dname, "DOMAIN_REGION": dregion}))

    # 4g. Small-data areas (arch-specific)
    if flags["HAVE_SMALL_DATA"]:
        out.append(read_fragment(os.path.join(args.arch_dir, "small_data.ld.in")))

    # 4h. Kernel stacks
    out.append(read_fragment(os.path.join(args.kernel_dir, "kernel_stacks.ld.in")))

    # 4i. CSA pool (arch-specific)
    if flags["HAVE_CSA"]:
        out.append(read_fragment(os.path.join(args.arch_dir, "csa_pool.ld.in")))

    # 4j. User pool (always last in KERNEL_RAM)
    out.append(read_fragment(os.path.join(args.kernel_dir, "user_pool.ld.in")))

    out.append("\n} /* SECTIONS */\n")

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write("\n".join(out))

    print(f"[generate_ld.py] wrote {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
