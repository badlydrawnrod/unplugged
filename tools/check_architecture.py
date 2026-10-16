#!/usr/bin/env python3
"""Flag obvious implementation includes from public headers."""

import os
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inc"}
INCLUDE = re.compile(r'^\s*#\s*include\s*(?:"([^"\n]+)"|<([^>\n]+)>)')


@dataclass(frozen=True)
class RepositoryInventory:
    """Active public headers and Bazel package roots, using repository paths."""

    headers: tuple[tuple[PurePosixPath, Path], ...]
    package_roots: tuple[PurePosixPath, ...]


def repository_inventory(root):
    """Discover active headers and package roots without traversing build output."""
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root, check=True, capture_output=True)

    active_paths = set()
    for raw_path in result.stdout.split(b"\0"):
        if raw_path:
            # Git emits repository paths with forward slash separators on every
            # platform. Keep that spelling for comparisons and diagnostics.
            active_paths.add(PurePosixPath(os.fsdecode(raw_path)))

    headers = []
    package_roots = set()
    for relative in sorted(active_paths, key=PurePosixPath.as_posix):
        full_path = root.joinpath(*relative.parts)
        if not full_path.is_file() or full_path.is_symlink():
            continue
        if relative.name in {"BUILD", "BUILD.bazel"}:
            package_roots.add(relative.parent)
        if relative.suffix in HEADER_SUFFIXES and "include" in relative.parts:
            headers.append((relative, full_path))

    return RepositoryInventory(
        headers=tuple(headers),
        package_roots=tuple(sorted(package_roots, key=PurePosixPath.as_posix)),
    )


def check(root):
    """Return (violations, operational error)."""
    try:
        inventory = repository_inventory(root)
    except (OSError, subprocess.CalledProcessError) as error:
        detail = getattr(error, "stderr", b"")
        if isinstance(detail, bytes):
            detail = os.fsdecode(detail).strip()
        return (), f"Git file discovery failed: {detail or error}"

    findings = []
    for relative, path in inventory.headers:
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError) as error:
            return (), f"Cannot read {relative.as_posix()}: {error}"
        for line_number, line in enumerate(lines, start=1):
            match = INCLUDE.match(line)
            if not match:
                continue
            spelling = match.group(1) or match.group(2)
            parts = PurePosixPath(spelling).parts
            if "impl" in parts or ".." in parts:
                findings.append((relative.as_posix(), line_number, spelling))
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
