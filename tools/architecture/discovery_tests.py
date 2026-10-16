#!/usr/bin/env python3
"""Exercise discovery against real isolated Git repositories."""

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from discovery import discover_files, Inventory
from facts import OperationalError


class DiscoveryTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.git("init", "--quiet")

    def git(self, *args):
        return subprocess.run(["git", *args], cwd=self.root, check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def write(self, relative, content=""):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def test_active_tracked_untracked_and_tracked_ignored_inputs(self):
        self.write("tracked.cpp")
        self.write("ignored/tracked.h")
        self.write("deleted.h")
        self.write("deleted/BUILD")
        self.git("add", ".")
        (self.root / "deleted.h").unlink()
        (self.root / "deleted/BUILD").unlink()
        self.write(".gitignore", "ignored/\noutput/\n")
        self.write("untracked.h")
        self.write("ignored/untracked.h")
        self.write("output/BUILD.bazel")
        self.write("output/generated.cpp")
        self.assertEqual(discover_files(self.root), Inventory(
            ("ignored/tracked.h", "tracked.cpp", "untracked.h"), (), ()))

    def test_supported_suffixes_and_nul_delimited_names(self):
        names = {f"source{suffix}" for suffix in (
            ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc")}
        names.update({"space name.cpp", "line\nbreak.h", "white space/also\nsource.cc"})
        for name in names | {"README.md", "unsupported.C", "looks.cpp.txt"}:
            self.write(name)
        self.git("add", "space name.cpp", "line\nbreak.h")
        inventory = discover_files(self.root)
        self.assertEqual(inventory.sources, tuple(sorted(names)))
        self.assertEqual(inventory, discover_files(self.root))

    def test_build_inventory_retains_both_names_and_unique_packages(self):
        builds = ("BUILD", "BUILD.bazel", "component/BUILD.bazel",
                  "component/nested/BUILD", "package with space/BUILD")
        for name in builds:
            self.write(name)
        self.write("unpackaged/source.cpp")
        self.write("component/BUILD.txt")
        inventory = discover_files(self.root)
        self.assertEqual(inventory.build_files, tuple(sorted(builds)))
        self.assertEqual(inventory.packages,
                         ("", "component", "component/nested", "package with space"))

    def test_symlink_files_and_replaced_tracked_directories_are_excluded(self):
        original = self.write("original/value.h")
        self.write("original/BUILD")
        self.write("tracked.h")
        self.write("replaced/value.h")
        self.write("replaced/BUILD")
        self.git("add", ".")
        (self.root / "tracked.h").unlink()
        (self.root / "tracked.h").symlink_to(original)
        (self.root / "untracked.h").symlink_to(original)
        (self.root / "BUILD").symlink_to(self.root / "original/BUILD")
        (self.root / "replaced/value.h").unlink()
        (self.root / "replaced/BUILD").unlink()
        (self.root / "replaced").rmdir()
        (self.root / "replaced").symlink_to(self.root / "original", target_is_directory=True)
        (self.root / "linked").symlink_to(self.root / "original", target_is_directory=True)
        self.assertEqual(discover_files(self.root), Inventory(
            ("original/value.h",), ("original/BUILD",), ("original",)))

    def test_directory_replaced_by_regular_file_is_excluded(self):
        source = self.write("replaced/value.h")
        self.git("add", ".")
        source.unlink()
        source.parent.rmdir()
        self.write("replaced")
        self.assertEqual(discover_files(self.root), Inventory((), (), ()))

    def test_non_repository_is_an_operational_error(self):
        with tempfile.TemporaryDirectory() as outside:
            with self.assertRaisesRegex(OperationalError, "Git file discovery failed"):
                discover_files(Path(outside))

    def test_git_execution_failure_is_an_operational_error(self):
        with patch("discovery.subprocess.run", side_effect=OSError("cannot execute")):
            with self.assertRaisesRegex(OperationalError, "Cannot run Git"):
                discover_files(self.root)

    def test_unreadable_required_input_is_an_operational_error(self):
        self.write("source.cpp")
        # Use an injected read failure because elevated test users can bypass modes.
        with patch.object(Path, "open", side_effect=PermissionError("unreadable")):
            with self.assertRaisesRegex(OperationalError, "Cannot inspect 'source.cpp'"):
                discover_files(self.root)


if __name__ == "__main__":
    unittest.main()
