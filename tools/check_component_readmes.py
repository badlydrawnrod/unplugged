#!/usr/bin/env python3
"""Require a README for each top-level public library component."""

from pathlib import Path
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent


def public_components(root):
    """Return top-level directories with both BUILD.bazel and include/."""
    return tuple(sorted(
        (path for path in root.iterdir()
         if path.is_dir()
         and (path / "BUILD.bazel").is_file()
         and (path / "include").is_dir()),
        key=lambda path: path.name,
    ))


def missing_readmes(root):
    """Return repository-relative names of public components without README."""
    return tuple(path.name for path in public_components(root)
                 if not (path / "README.md").is_file())


def main():
    missing = missing_readmes(REPO_ROOT)
    if missing:
        for component in missing:
            print(f"{component}/README.md: public component is missing README.md")
        return 1
    print("Every top-level public component has a README.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
