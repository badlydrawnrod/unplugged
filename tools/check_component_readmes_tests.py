#!/usr/bin/env python3
"""Focused filesystem fixtures for the component README check."""

from contextlib import redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import check_component_readmes as checker


class ComponentReadmeCheckTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def write(self, relative):
        target = self.root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("", encoding="utf-8")
        return target

    def test_public_component_requires_readme(self):
        self.write("document/BUILD.bazel")
        (self.root / "document/include").mkdir(parents=True)

        self.assertEqual(checker.missing_readmes(self.root), ("document",))

    def test_public_component_with_readme_passes(self):
        self.write("document/BUILD.bazel")
        (self.root / "document/include").mkdir(parents=True)
        self.write("document/README.md")

        self.assertEqual(checker.missing_readmes(self.root), ())

    def test_requires_both_build_file_and_include_directory(self):
        self.write("build_only/BUILD.bazel")
        (self.root / "headers_only/include").mkdir(parents=True)

        self.assertEqual(checker.public_components(self.root), ())
        self.assertEqual(checker.missing_readmes(self.root), ())

    def test_only_checks_top_level_components(self):
        self.write("parent/BUILD.bazel")
        (self.root / "parent/include").mkdir(parents=True)
        self.write("parent/nested/BUILD.bazel")
        (self.root / "parent/nested/include").mkdir(parents=True)

        self.assertEqual(checker.public_components(self.root),
                         (self.root / "parent",))
        self.assertEqual(checker.missing_readmes(self.root), ("parent",))

    def test_diagnostic_names_each_missing_readme(self):
        for component in ("document", "key"):
            self.write(f"{component}/BUILD.bazel")
            (self.root / component / "include").mkdir(parents=True)

        output = io.StringIO()
        with patch.object(checker, "REPO_ROOT", self.root), \
                redirect_stdout(output):
            self.assertEqual(checker.main(), 1)
        self.assertEqual(
            output.getvalue(),
            "document/README.md: public component is missing README.md\n"
            "key/README.md: public component is missing README.md\n",
        )


if __name__ == "__main__":
    unittest.main()
