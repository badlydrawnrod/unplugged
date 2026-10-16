#!/usr/bin/env python3
"""Flag obvious implementation includes from public headers.

Only literal ``#include "..."`` and ``#include <...>`` operands are inspected.
Macro-based include operands are intentionally ignored.
"""

import os
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inc"}
PRIVATE_PATH_SEGMENTS = {"detail", "impl", "internal", "tests"}
INCLUDE = re.compile(r'^\s*#\s*include\s*(?:"([^"\n]+)"|<([^>\n]+)>)')


@dataclass(frozen=True)
class HeaderFile:
    """An active repository header with its normalized path and filesystem path."""

    relative: PurePosixPath
    path: Path


@dataclass(frozen=True)
class RepositoryInventory:
    """Active header indexes and Bazel package roots, using repository paths."""

    headers: tuple[HeaderFile, ...]
    package_roots: tuple[PurePosixPath, ...]
    headers_by_repository_path: dict[PurePosixPath, HeaderFile]
    headers_by_public_include_spelling: dict[
        PurePosixPath, tuple[HeaderFile, ...]]


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
    repository_headers = []
    package_roots = set()
    for relative in sorted(active_paths, key=PurePosixPath.as_posix):
        full_path = root.joinpath(*relative.parts)
        if not full_path.is_file() or full_path.is_symlink():
            continue
        if relative.name in {"BUILD", "BUILD.bazel"}:
            package_roots.add(relative.parent)
        if relative.suffix in HEADER_SUFFIXES:
            header = HeaderFile(relative=relative, path=full_path)
            repository_headers.append(header)
            if "include" in relative.parts:
                headers.append(header)

    include_spellings = {}
    for header in repository_headers:
        parts = header.relative.parts
        if "include" not in parts:
            continue
        include_index = parts.index("include")
        spelling = PurePosixPath(*parts[include_index + 1:])
        include_spellings.setdefault(spelling, []).append(header)

    return RepositoryInventory(
        headers=tuple(headers),
        package_roots=tuple(sorted(package_roots, key=PurePosixPath.as_posix)),
        headers_by_repository_path={
            header.relative: header for header in repository_headers
        },
        headers_by_public_include_spelling={
            spelling: tuple(sorted(matches,
                                   key=lambda item: item.relative.as_posix()))
            for spelling, matches in include_spellings.items()
        },
    )


def resolve_header(inventory, spelling):
    """Return matching repository headers for a repository or public spelling."""
    path = normalize_include_spelling(spelling)
    matches = {}
    repository_match = inventory.headers_by_repository_path.get(path)
    if repository_match is not None:
        matches[repository_match.relative] = repository_match
    for header in inventory.headers_by_public_include_spelling.get(path, ()):
        matches[header.relative] = header
    return tuple(matches[key] for key in sorted(matches,
                                                key=PurePosixPath.as_posix))


def normalize_include_spelling(spelling):
    """Treat either slash style as a path separator for rule evaluation."""
    return PurePosixPath(spelling.replace("\\", "/"))


def path_segments(spelling):
    """Return normalized raw segments, preserving dot segments for checks."""
    return tuple(spelling.replace("\\", "/").split("/"))


def package_root_for(inventory, header):
    """Return the nearest Bazel package root containing a repository header."""
    relative_parts = header.relative.parts
    candidates = [
        package for package in inventory.package_roots
        if relative_parts[:len(package.parts)] == package.parts
    ]
    if not candidates:
        return None
    return max(candidates, key=lambda package: len(package.parts))


def component_for(header):
    """Return the repository's top-level component containing a header."""
    return header.relative.parts[0] if header.relative.parts else None


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
    for header in inventory.headers:
        try:
            lines = header.path.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError) as error:
            return (), f"Cannot read {header.relative.as_posix()}: {error}"
        for line_number, line in enumerate(lines, start=1):
            match = INCLUDE.match(line)
            if not match:
                continue
            spelling = match.group(1) or match.group(2)
            segments = path_segments(spelling)
            if "impl" in segments:
                findings.append((header.relative.as_posix(), line_number,
                                 spelling, "public header includes an impl path"))
            if "." in segments or ".." in segments:
                findings.append((header.relative.as_posix(), line_number,
                                 spelling, "public header include uses path traversal"))

            matches = resolve_header(inventory, spelling)
            private_segments = PRIVATE_PATH_SEGMENTS & set(segments)
            if len(matches) > 1:
                private_targets = any(
                    PRIVATE_PATH_SEGMENTS & set(target.relative.parts)
                    or target.relative.stem.endswith("_tests")
                    for target in matches
                )
                if private_segments or private_targets:
                    findings.append((header.relative.as_posix(), line_number,
                                     spelling,
                                     "ambiguous private include spelling"))
                continue
            if not matches:
                # Unresolved include paths may refer to system or external
                # headers. Ownership rules apply only to indexed repository files.
                continue

            target = matches[0]
            target_segments = set(target.relative.parts)
            if ("tests" in segments or "tests" in target_segments
                    or target.relative.stem.endswith("_tests")):
                findings.append((header.relative.as_posix(), line_number,
                                 spelling, "public header includes a test header"))
            if "internal" in target_segments:
                source_component = component_for(header)
                target_component = component_for(target)
                if (source_component is None or target_component is None
                        or source_component != target_component):
                    findings.append((header.relative.as_posix(), line_number,
                                     spelling,
                                     "public header includes an internal header from another component"))
            if "detail" in target_segments:
                source_package = package_root_for(inventory, header)
                target_package = package_root_for(inventory, target)
                if (source_package is None or target_package is None
                        or source_package != target_package):
                    findings.append((header.relative.as_posix(), line_number,
                                     spelling,
                                     "public header includes a detail header from another Bazel package"))
    return tuple(findings), None


def main():
    findings, error = check(REPO_ROOT)
    if error:
        print(f"Architecture check error: {error}", file=sys.stderr)
        return 2
    for path, line, spelling, reason in findings:
        print(f"{path}:{line}: {reason} {spelling!r}")
    if findings:
        return 1
    print("No suspicious includes in public headers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
