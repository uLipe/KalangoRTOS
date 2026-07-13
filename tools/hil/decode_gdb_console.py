#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Decode g_ulmk_console_log dump from tricore-elf-gdb batch output."""

from __future__ import annotations

import re
import sys


def decode_gdb_console(text: str, max_len: int = 2048) -> str:
	idx = text.find("g_ulmk_console_log>:")
	if idx < 0:
		idx = text.find("g_ulmk_console_log")
	chunk = text[idx:] if idx >= 0 else text
	chars: list[str] = []
	for m in re.finditer(r"(-?\d+)\s+'((?:\\.|[^'\\])*)'", chunk):
		b = int(m.group(1)) & 0xFF
		if b == 0:
			break
		chars.append(chr(b))
		if len(chars) >= max_len:
			break
	return "".join(chars)


def main() -> int:
	path = sys.argv[1] if len(sys.argv) > 1 else "-"
	text = sys.stdin.read() if path == "-" else open(path, errors="replace").read()
	sys.stdout.write(decode_gdb_console(text))
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
