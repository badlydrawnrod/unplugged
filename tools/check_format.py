#!/usr/bin/env python3
"""Check active C++ sources against the repository's clang-format configuration."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


FORMATTER_VERSION = "21.1.8"
REPO_ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", default="clang-format",
                        help="clang-format executable (default: %(default)s)")
    args = parser.parse_args()

    try:
        version = subprocess.check_output(
            [args.clang_format, "--version"], text=True)
        match = re.search(r"\bversion (\d+\.\d+\.\d+)\b", version)
        if match is None or match.group(1) != FORMATTER_VERSION:
            print(f"Expected clang-format {FORMATTER_VERSION}; got {version.strip()}",
                  file=sys.stderr)
            return 1

        # Git excludes ignored build output without traversing Bazel symlinks.
        # Include untracked sources so the check also covers new work.
        paths = subprocess.check_output(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             "-z", "--", "*.cpp", "*.h", ":(exclude)old_20260924/**"],
            cwd=REPO_ROOT)
        files = sorted({os.fsdecode(path) for path in paths.split(b"\0") if path})
        if not files:
            print("No active C++ sources found.", file=sys.stderr)
            return 1

        result = subprocess.run(
            [args.clang_format, f"--style=file:{REPO_ROOT / '.clang-format'}",
             "--dry-run", "--Werror", "--", *files], cwd=REPO_ROOT)
        if result.returncode == 0:
            print(f"Formatting passed for {len(files)} C++ files.")
        return result.returncode
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Formatting check failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
