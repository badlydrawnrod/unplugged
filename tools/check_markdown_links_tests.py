#!/usr/bin/env python3
"""Focused filesystem fixtures for the Markdown link check."""

from pathlib import Path
import subprocess
import tempfile
import unittest

import check_markdown_links as checker


class MarkdownLinkCheckTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)

    def write(self, relative, contents=""):
        target = self.root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(contents, encoding="utf-8")
        return target

    def test_file_directory_and_reference_links_pass(self):
        self.write("docs/README.md", (
            "[contract](../api/contract.md#usage)\n"
            "[examples](../examples/)\n"
            "[versioned API](<../api docs/(v1).md>)\n"
            "[reference]: ../api/contract.md\n"
            "[the API][reference]\n"
        ))
        self.write("api/contract.md")
        self.write("api docs/(v1).md")
        (self.root / "examples").mkdir()

        self.assertEqual(checker.missing_links(self.root), ())

    def test_missing_inline_and_reference_links_include_line_numbers(self):
        self.write("docs/README.md", (
            "# Links\n"
            "[missing](absent.md)\n"
            "[also missing]: absent-directory/\n"
        ))

        self.assertEqual(
            checker.missing_links(self.root),
            (("docs/README.md", 2, "absent.md"),
             ("docs/README.md", 3, "absent-directory/")),
        )

    def test_external_anchor_and_root_absolute_links_are_ignored(self):
        self.write("README.md", (
            "[external](https://example.com/path)\n"
            "[anchor](#section)\n"
            "[root absolute](/outside/repository)\n"
        ))

        self.assertEqual(checker.missing_links(self.root), ())

    def test_links_in_fenced_code_are_ignored(self):
        self.write("README.md", (
            "```markdown\n"
            "[example](missing.md)\n"
            "```\n"
            "[real](present.md)\n"
        ))
        self.write("present.md")

        self.assertEqual(checker.missing_links(self.root), ())

    def test_discovery_includes_untracked_and_excludes_ignored_files(self):
        self.write("README.md", "[missing](absent.md)\n")
        self.write(".gitignore", "ignored/\n")
        self.write("ignored/README.md", "[missing](absent.md)\n")

        self.assertEqual(checker.discover_markdown_files(self.root),
                         (Path("README.md"),))
        self.assertEqual(len(checker.missing_links(self.root)), 1)


if __name__ == "__main__":
    unittest.main()
