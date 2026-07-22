#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Run QEMU for an SDK suite case and stop as soon as every PASS sentinel
appears (or a FAIL sentinel), instead of burning the full QEMU_TIMEOUT.

Exit status is always 0 when the process was managed cleanly — the Makefile
still greps the log for sentinels / FAIL.  Non-zero only on runner errors
(missing args, QEMU failed to start).
"""

from __future__ import annotations

import argparse
import os
import select
import signal
import subprocess
import sys
import time


def _strip_quotes(s: str) -> str:
    if len(s) >= 2 and s[0] == s[-1] and s[0] in ("'", '"'):
        return s[1:-1]
    return s


def _kill_pg(proc: subprocess.Popen[bytes]) -> None:
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.wait(timeout=2)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--timeout", type=float, required=True,
            help="Maximum seconds to wait for sentinels")
    ap.add_argument("--log", required=True, help="Output log path")
    ap.add_argument("--fail-sentinel", default="",
            help="If this string appears, stop early (FAIL)")
    ap.add_argument("--sentinel", action="append", default=[],
            help="PASS sentinel (repeatable); all must appear")
    ap.add_argument("cmd", nargs=argparse.REMAINDER,
            help="QEMU command after --")
    args = ap.parse_args()

    cmd = args.cmd
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        print("qemu_until_sentinels: missing QEMU command", file=sys.stderr)
        return 2

    sentinels = [_strip_quotes(s) for s in args.sentinel if s]
    fail_sentinel = _strip_quotes(args.fail_sentinel) if args.fail_sentinel else ""
    pending = set(sentinels)
    deadline = time.monotonic() + args.timeout

    with open(args.log, "wb") as logf:
        try:
            proc = subprocess.Popen(
                cmd,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
                start_new_session=True,
            )
        except OSError as exc:
            print(f"qemu_until_sentinels: failed to start: {exc}",
                  file=sys.stderr)
            return 2

        assert proc.stdout is not None
        fd = proc.stdout.fileno()
        buf = bytearray()
        text = ""
        reason = "timeout"

        try:
            while True:
                now = time.monotonic()
                if now >= deadline:
                    reason = "timeout"
                    break
                if proc.poll() is not None:
                    rest = proc.stdout.read()
                    if rest:
                        buf.extend(rest)
                        logf.write(rest)
                        logf.flush()
                        sys.stdout.buffer.write(rest)
                        sys.stdout.buffer.flush()
                        text = buf.decode("utf-8", errors="replace")
                    reason = "qemu-exit"
                    break

                ready, _, _ = select.select([fd], [], [],
                               min(0.25, deadline - now))
                if not ready:
                    continue

                try:
                    chunk = os.read(fd, 4096)
                except OSError:
                    chunk = b""
                if not chunk:
                    continue

                buf.extend(chunk)
                logf.write(chunk)
                logf.flush()
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()

                text = buf.decode("utf-8", errors="replace")
                if fail_sentinel and fail_sentinel in text:
                    reason = "fail-sentinel"
                    break
                if pending:
                    pending = {s for s in pending if s not in text}
                    if not pending:
                        reason = "sentinels"
                        break
        finally:
            _kill_pg(proc)

    print(f"\n--- qemu stopped ({reason}) ---", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
