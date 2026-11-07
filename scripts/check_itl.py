#!/usr/bin/env python3
"""Differential check of the Rust prayer engine against the bundled ITL C code.

Usage: check_itl.py /path/to/itl_ref /path/to/namaz-cli [cases]
"""
from __future__ import annotations

import random
import subprocess
import sys

METHODS = range(0, 12)
DATES = [
    (2026, 1, 15),
    (2026, 3, 20),
    (2026, 6, 21),
    (2026, 9, 23),
    (2026, 12, 21),
    (2025, 2, 28),
    (2024, 2, 29),
]
INVALID = {"99:99:00", "--:--:--"}


def normalize(value: str) -> str:
    return "INVALID" if value in INVALID else value


def run(binary: str, args: list[str]) -> list[str]:
    result = subprocess.run(
        [binary, *args], check=True, capture_output=True, text=True
    )
    return result.stdout.splitlines()


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    reference, rust = sys.argv[1], sys.argv[2]
    cases = int(sys.argv[3]) if len(sys.argv) > 3 else 1500

    random.seed(7)
    for index in range(cases):
        latitude = round(random.uniform(-89.5, 89.5), 4)
        longitude = round(random.uniform(-180.0, 180.0), 4)
        timezone = random.choice([-8, -5, 0, 1, 3, 5.5, 7, 9, 10, 12])
        method = random.choice(list(METHODS))
        year, month, day = random.choice(DATES)
        args = [
            str(latitude),
            str(longitude),
            str(timezone),
            str(method),
            str(year),
            str(month),
            str(day),
        ]
        expected = [normalize(value) for value in run(reference, args)[:6]]
        got = [
            normalize(line.split()[-1])
            for line in run(rust, args)[:6]
        ]
        if expected != got:
            print(f"mismatch at case {index + 1}: {' '.join(args)}")
            print(f"  ITL : {expected}")
            print(f"  Rust: {got}")
            return 1
    print(f"prayer engine matches ITL for {cases} cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
