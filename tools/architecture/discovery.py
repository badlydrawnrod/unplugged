"""Find active first-party inputs without traversing ignored or linked trees."""

from dataclasses import dataclass
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess

from facts import OperationalError


SOURCE_SUFFIXES = frozenset({
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc",
})
BUILD_NAMES = frozenset({"BUILD", "BUILD.bazel"})


@dataclass(frozen=True)
class Inventory:
    sources: tuple[str, ...]
    build_files: tuple[str, ...]
    packages: tuple[str, ...]


def _active_file(root: Path, relative: str) -> bool:
    """Reject symlinks at every level, including tracked directories replaced by links."""
    path = root
    parts = PurePosixPath(relative).parts
    try:
        for index, part in enumerate(parts):
            path = path / part
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode):
                return False
            if index < len(parts) - 1:
                if not stat.S_ISDIR(mode):
                    return False
            elif not stat.S_ISREG(mode):
                return False
        # Required inputs that cannot be read must not silently disappear.
        with path.open("rb"):
            pass
    except (FileNotFoundError, NotADirectoryError):
        return False
    except OSError as error:
        raise OperationalError(f"Cannot inspect {relative!r}: {error}") from error
    return True


def discover_files(root: Path) -> Inventory:
    """Inventory a Git repository root, retaining both BUILD names when present.

    Paths are sorted repository-relative POSIX strings. Cached inputs remain
    visible even if newly ignored; ignored untracked output is excluded by Git.
    Packages are unique; Bazel itself chooses BUILD.bazel over BUILD when both
    exist in the same package.
    """
    try:
        result = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
            cwd=root, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as error:
        detail = os.fsdecode(error.stderr).strip()
        raise OperationalError(f"Git file discovery failed: {detail}") from error
    except OSError as error:
        raise OperationalError(f"Cannot run Git file discovery: {error}") from error

    sources = set()
    build_files = set()
    for relative in sorted({os.fsdecode(value) for value in result.stdout.split(b"\0")
                            if value}):
        path = PurePosixPath(relative)
        is_build = path.name in BUILD_NAMES
        if not is_build and path.suffix not in SOURCE_SUFFIXES:
            continue
        if not _active_file(root, relative):
            continue
        if is_build:
            build_files.add(relative)
        else:
            sources.add(relative)

    packages = {str(PurePosixPath(path).parent) for path in build_files}
    packages.discard(".")
    if build_files.intersection(BUILD_NAMES):
        packages.add("")
    return Inventory(tuple(sorted(sources)), tuple(sorted(build_files)),
                     tuple(sorted(packages)))
