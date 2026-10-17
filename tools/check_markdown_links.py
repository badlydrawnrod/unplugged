#!/usr/bin/env python3
"""Check repository-relative Markdown links to files and directories."""

from pathlib import Path
import os
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit


REPO_ROOT = Path(__file__).resolve().parent.parent
INLINE_LINK = re.compile(
    r"!?\[[^\]]*\]\(\s*(?:<([^>]+)>|([^\s)]+))")
REFERENCE_LINK = re.compile(
    r"^ {0,3}\[[^\]]+\]:\s*(?:<([^>]+)>|(\S+))", re.MULTILINE)
FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})(.*)$")


def discover_markdown_files(root):
    """List active tracked and untracked Markdown files without following symlinks."""
    output = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root)
    paths = {Path(os.fsdecode(value)) for value in output.split(b"\0") if value}
    return tuple(sorted(
        path for path in paths
        if path.suffix.lower() == ".md"
        and (root / path).is_file()
        and not any((root / parent).is_symlink()
                    for parent in (path, *path.parents))))


def mask_fenced_code(text):
    """Blank fenced code blocks while preserving offsets and line numbers."""
    lines = text.splitlines(keepends=True)
    masked = []
    fence_char = None
    fence_size = 0
    for line in lines:
        marker = FENCE.match(line.rstrip("\r\n"))
        if fence_char is None:
            if marker is not None:
                fence_char = marker.group(1)[0]
                fence_size = len(marker.group(1))
                masked.append("".join("\n" if char == "\n" else " "
                                      for char in line))
                continue
        else:
            if (marker is not None
                    and marker.group(1)[0] == fence_char
                    and len(marker.group(1)) >= fence_size
                    and not marker.group(2).strip()):
                fence_char = None
                fence_size = 0
            masked.append("".join("\n" if char == "\n" else " "
                                  for char in line))
            continue
        masked.append(line)
    return "".join(masked)


def link_destinations(text):
    """Yield destinations from inline links and reference definitions."""
    text = mask_fenced_code(text)
    matches = list(INLINE_LINK.finditer(text))
    matches.extend(REFERENCE_LINK.finditer(text))
    for match in sorted(matches, key=lambda item: item.start()):
        destination = match.group(1) or match.group(2)
        yield destination, text.count("\n", 0, match.start()) + 1


def missing_links(root, files=None):
    """Return (source, line, destination) for missing local link targets."""
    if files is None:
        files = discover_markdown_files(root)
    findings = []
    for source in files:
        source_path = root / source
        text = source_path.read_text(encoding="utf-8")
        for destination, line in link_destinations(text):
            parsed = urlsplit(destination)
            if parsed.scheme or parsed.netloc or destination.startswith("/"):
                continue
            if not parsed.path:
                continue
            target = source_path.parent / unquote(parsed.path)
            if not target.exists():
                findings.append((source.as_posix(), line, destination))
    return tuple(findings)


def main():
    try:
        findings = missing_links(REPO_ROOT)
    except (OSError, subprocess.CalledProcessError, UnicodeError) as error:
        print(f"Markdown link check failed: {error}", file=sys.stderr)
        return 1
    if findings:
        for source, line, destination in findings:
            print(f"{source}:{line}: missing Markdown link target: {destination}")
        return 1
    print("Repository-relative Markdown link targets exist.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
