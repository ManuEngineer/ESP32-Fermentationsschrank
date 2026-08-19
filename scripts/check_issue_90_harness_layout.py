#!/usr/bin/env python3
"""Check that Issue-90 scratch is bounded static storage, not task-stack data."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
from pathlib import Path


MAX_SCRATCH_BYTES = 16 * 1024
SYMBOL = "gHarnessScratch"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path,
                        default=Path("main/issue_90_nvs_hardware_verification.cpp"))
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--stack-usage", type=Path)
    parser.add_argument("--max-function-stack", type=int, default=4096)
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")
    required = (
        "HarnessScratch gHarnessScratch{}",
        "static_assert(\n    sizeof(HarnessScratch) <= kHarnessScratchBudgetBytes",
        "std::array<PageEvidence, kPageCount> previousPages_",
    )
    if required[0] not in source or required[1] not in source:
        raise SystemExit("FAIL harness scratch is not a bounded static object")
    if required[2] in source:
        raise SystemExit("FAIL PageEvidence arrays remain automatic members")
    if args.elf is not None:
        nm = shutil.which("xtensa-esp-elf-nm") or shutil.which("nm")
        if nm is None:
            raise SystemExit("FAIL no nm available for ELF scratch proof")
        result = subprocess.run(
            [nm, "-S", "--defined-only", str(args.elf)],
            text=True, capture_output=True, check=False,
        )
        if result.returncode != 0:
            raise SystemExit(f"FAIL nm: {result.stderr.strip()}")
        line = next((line for line in result.stdout.splitlines()
                     if re.search(rf"{SYMBOL}E?$", line)), None)
        if line is None:
            raise SystemExit("FAIL gHarnessScratch is absent from the ELF")
        fields = line.split()
        if len(fields) < 2:
            raise SystemExit(f"FAIL cannot parse scratch symbol: {line}")
        try:
            size = int(fields[1], 16)
        except ValueError as error:
            raise SystemExit(f"FAIL cannot parse scratch size: {line}") from error
        if size > MAX_SCRATCH_BYTES:
            raise SystemExit(f"FAIL scratch size {size} exceeds {MAX_SCRATCH_BYTES}")
        print(f"PASS harness-layout static-symbol={SYMBOL} bytes={size}")
    if args.stack_usage is not None:
        if not args.stack_usage.is_file():
            raise SystemExit(f"FAIL stack-usage file missing: {args.stack_usage}")
        largest = 0
        largest_function = "unknown"
        for line in args.stack_usage.read_text(encoding="utf-8").splitlines():
            fields = line.rsplit("\t", 2)
            if len(fields) != 3 or not fields[1].isdigit():
                continue
            stack = int(fields[1])
            if stack > largest:
                largest, largest_function = stack, fields[0]
        if largest > args.max_function_stack:
            raise SystemExit(
                f"FAIL harness function stack {largest} exceeds "
                f"{args.max_function_stack}: {largest_function}"
            )
        print(f"PASS harness-stack largest_function_bytes={largest}")
    if args.elf is None and args.stack_usage is None:
        print("PASS harness-layout source-static-bounded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
