#!/usr/bin/env python3
"""Aggregate CI test JSONL + coverage_manifest.yaml → markdown report."""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

try:
    import yaml
except ImportError:
    yaml = None  # type: ignore


ARCHES = ("tricore", "riscv", "arm")
RESULT_RANK = {"FAIL": 3, "PARTIAL": 2, "PASS": 1, "SKIP": 0, "n/a": -1}


def _load_yaml(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    if yaml is not None:
        return yaml.safe_load(text)
    # Minimal fallback: not full YAML — require PyYAML in CI image.
    raise SystemExit(
        "PyYAML required: pip install pyyaml (or use container with PyYAML)"
    )


def _load_jsonl(dir_path: Path) -> list[dict]:
    rows: list[dict] = []
    if not dir_path.is_dir():
        return rows
    for p in sorted(dir_path.rglob("*.jsonl")):
        for line in p.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line:
                continue
            rows.append(json.loads(line))
    return rows


def _agg_status(statuses: list[str]) -> str:
    if not statuses:
        return "n/a"
    if any(s == "FAIL" for s in statuses):
        if any(s == "PASS" for s in statuses):
            return "PARTIAL"
        return "FAIL"
    if any(s == "PARTIAL" for s in statuses):
        return "PARTIAL"
    if all(s in ("PASS", "SKIP", "n/a") for s in statuses) and any(
        s == "PASS" for s in statuses
    ):
        return "PASS"
    if all(s == "SKIP" for s in statuses):
        return "SKIP"
    return "PARTIAL"


def _case_matrix(rows: list[dict]) -> tuple[list[str], dict[str, dict[str, str]]]:
    """Return (sorted case names, case -> arch/host -> status)."""
    bucket: dict[str, dict[str, list[str]]] = defaultdict(
        lambda: defaultdict(list)
    )

    for r in rows:
        case = r.get("case", "?")
        status = r.get("status", "FAIL")
        arch = r.get("arch", "unknown")
        kind = r.get("kind", "")
        if kind == "unit" or arch == "host":
            bucket[case]["host"].append(status)
            continue
        if arch not in ARCHES:
            if "mps2" in str(r.get("board", "")) or arch.startswith("arm"):
                arch = "arm"
            else:
                continue
        bucket[case][arch].append(status)

    cases = sorted(bucket.keys())
    matrix: dict[str, dict[str, str]] = {}
    for case in cases:
        matrix[case] = {"host": "n/a"}
        for a in ARCHES:
            matrix[case][a] = "n/a"
        for key, sts in bucket[case].items():
            matrix[case][key] = _agg_status(sts)
    return cases, matrix


def _md_escape(s: str) -> str:
    return s.replace("|", "\\|")


def generate(manifest: dict, rows: list[dict]) -> str:
    cases, matrix = _case_matrix(rows)
    lines: list[str] = []
    lines.append("# ulmk CI test report")
    lines.append("")
    lines.append(f"Cases with results: **{len(cases)}**")
    lines.append("")

    lines.append("## 1. Execution results")
    lines.append("")
    lines.append("| Case | Host (unit) | TriCore | RISC-V | ARM |")
    lines.append("|------|-------------|---------|--------|-----|")
    for case in cases:
        m = matrix[case]
        lines.append(
            f"| {_md_escape(case)} | {m.get('host', 'n/a')} | "
            f"{m.get('tricore', 'n/a')} | {m.get('riscv', 'n/a')} | "
            f"{m.get('arm', 'n/a')} |"
        )
    if not cases:
        lines.append("| *(no JSONL results)* | n/a | n/a | n/a | n/a |")
    lines.append("")
    lines.append(
        "Values: `PASS` | `FAIL` | `PARTIAL` | `SKIP` | `n/a`. "
        "ARM aggregates AN500+AN505 (mixed → PARTIAL)."
    )
    lines.append("")

    lines.append("## 2. Coverage levels")
    lines.append("")
    lines.append("| Area | TriCore | RISC-V | ARM | Notes |")
    lines.append("|------|---------|--------|-----|-------|")
    for area in manifest.get("areas", []):
        aid = area.get("id", "?")
        note = area.get("note", "")
        lines.append(
            f"| {_md_escape(aid)} | {area.get('tricore', 'missing')} | "
            f"{area.get('riscv', 'missing')} | {area.get('arm', 'missing')} | "
            f"{_md_escape(note)} |"
        )
    lines.append("")
    lines.append(
        "Values: `covered` | `partial` | `missing` "
        "(from `tools/coverage_manifest.yaml`, independent of run PASS/FAIL)."
    )
    lines.append("")

    # Summary counts for coverage
    cov_counts = {a: {"covered": 0, "partial": 0, "missing": 0} for a in ARCHES}
    for area in manifest.get("areas", []):
        for a in ARCHES:
            lvl = area.get(a, "missing")
            if lvl not in cov_counts[a]:
                lvl = "missing"
            cov_counts[a][lvl] += 1
    lines.append("### Coverage summary")
    lines.append("")
    lines.append("| Arch | covered | partial | missing |")
    lines.append("|------|---------|---------|---------|")
    for a in ARCHES:
        c = cov_counts[a]
        lines.append(
            f"| {a} | {c['covered']} | {c['partial']} | {c['missing']} |"
        )
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--manifest",
        type=Path,
        default=Path(__file__).resolve().parent / "coverage_manifest.yaml",
    )
    ap.add_argument(
        "--results-dir",
        type=Path,
        required=True,
        help="Directory of *.jsonl result files from test jobs",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=Path("test-report.md"),
    )
    args = ap.parse_args()

    manifest = _load_yaml(args.manifest)
    rows = _load_jsonl(args.results_dir)
    md = generate(manifest, rows)
    args.out.write_text(md, encoding="utf-8")
    print(f"Wrote {args.out} ({len(rows)} result rows, {len(manifest.get('areas', []))} areas)")


if __name__ == "__main__":
    main()
