#!/usr/bin/env python3
"""Strict ARM/TRIP/RESTORE adapter for an owner-supplied power controller."""

from __future__ import annotations

import os
import shlex
import subprocess
import sys


EXPECTED = {"ARM": "ARMED", "TRIP": "TRIPPED", "RESTORE": "RESTORED"}


def main() -> int:
    line = sys.stdin.readline().strip()
    if line.startswith("ARM token=") and line[len("ARM token="):]:
        action = "ARM"
        token = line[len("ARM token="):]
        if any(char.isspace() for char in token) or "=" in token:
            raise SystemExit("invalid ARM token")
    elif line in ("TRIP", "RESTORE"):
        action = line
        token = "-"
    else:
        raise SystemExit("expected exactly ARM token=<token>, TRIP, or RESTORE")
    if sys.stdin.readline() != "":
        raise SystemExit("expected exactly one power-controller command")
    command = os.environ.get("ISSUE90_POWER_CONTROLLER")
    if not command:
        raise SystemExit(
            "ISSUE90_POWER_CONTROLLER is required; no power controller is "
            "assumed or silently bypassed"
        )
    result = subprocess.run(
        [*shlex.split(command), action, token],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0 or result.stdout.strip() != EXPECTED[action]:
        raise SystemExit(
            f"power controller protocol failure: expected {EXPECTED[action]!r}, "
            f"got rc={result.returncode} stdout={result.stdout!r}"
        )
    sys.stdout.write(EXPECTED[action] + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
