#!/usr/bin/env python3
"""Run repository formatting, acceptance traceability, and checker fixture tests."""

import argparse
from pathlib import Path
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", default="clang-format")
    args = parser.parse_args()
    commands = [
        ["tools/check_format.py", "--clang-format", args.clang_format],
        ["tools/acceptance/check_traceability.py"],
        ["tools/acceptance/check_traceability_tests.py"],
    ]
    failed = False
    for command in commands:
        result = subprocess.run([sys.executable, *command], cwd=REPO_ROOT)
        failed |= result.returncode != 0
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
