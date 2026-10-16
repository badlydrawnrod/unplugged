#!/usr/bin/env python3
"""Flag obvious implementation includes from public headers."""

import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inc"}
INCLUDE = re.compile(r'^\s*#\s*include\s*(?:"([^"\n]+)"|<([^>\n]+)>)')


def public_headers(root):
    """Return active Git-known headers beneath conventional include directories."""
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root, check=True, capture_output=True)
    headers = []
    for raw_path in result.stdout.split(b"\0"):
        if not raw_path:
            continue
        relative = os.fsdecode(raw_path)
        path = PurePosixPath(relative)
        if path.suffix not in HEADER_SUFFIXES or "include" not in path.parts:
            continue
        full_path = root / relative
        if full_path.is_file() and not full_path.is_symlink():
            headers.append((relative, full_path))
    return sorted(headers)


def check(root):
    """Return (violations, operational error)."""
    try:
        headers = public_headers(root)
    except (OSError, subprocess.CalledProcessError) as error:
        detail = getattr(error, "stderr", b"")
        if isinstance(detail, bytes):
            detail = os.fsdecode(detail).strip()
        return (), f"Git file discovery failed: {detail or error}"

    findings = []
    for relative, path in headers:
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError) as error:
            return (), f"Cannot read {relative}: {error}"
        for line_number, line in enumerate(lines, start=1):
            match = INCLUDE.match(line)
            if not match:
                continue
            spelling = match.group(1) or match.group(2)
            parts = PurePosixPath(spelling).parts
            if "impl" in parts or ".." in parts:
                findings.append((relative, line_number, spelling))
    return tuple(findings), None


def main():
    findings, error = check(REPO_ROOT)
    if error:
        print(f"Architecture check error: {error}", file=sys.stderr)
        return 2
    for path, line, spelling in findings:
        print(f"{path}:{line}: public header includes suspicious path {spelling!r}")
    if findings:
        return 1
    print("No suspicious includes in public headers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
