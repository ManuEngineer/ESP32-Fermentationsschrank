#!/usr/bin/env python3
"""Repository-relative adapter for an owner-supplied power controller.

The firmware and runner use only ARM/TRIP/RESTORE tokens.  A real controller
is deliberately external to the repository and is selected through the
ISSUE90_POWER_CONTROLLER environment variable.
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("ARM", "TRIP", "RESTORE"))
    parser.add_argument("token")
    args = parser.parse_args()
    command = os.environ.get("ISSUE90_POWER_CONTROLLER")
    if not command:
        raise SystemExit(
            "ISSUE90_POWER_CONTROLLER is required; no power controller is "
            "assumed or silently bypassed"
        )
    result = subprocess.run(
        [*shlex.split(command), args.action, args.token], check=False
    )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
