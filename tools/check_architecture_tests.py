#!/usr/bin/env python3
"""Focused architecture-checker fixtures using isolated temporary repositories."""

from pathlib import Path
import subprocess
import tempfile
import unittest

import check_architecture as checker


class ArchitectureCheckTest(unittest.TestCase):
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

    def findings(self):
        findings, error = checker.check(self.root)
        self.assertIsNone(error)
        return findings

    def reasons(self):
        return [finding[3] for finding in self.findings()]

    def test_same_component_internal_includes_are_allowed(self):
        self.write("document/BUILD.bazel")
        self.write("document/include/document/document.h", (
            '#include "document/internal/gap_buffer/gap_buffer.h"\n'
            '#include "document/internal/line_starts/line_starts.h"\n'
        ))
        self.write("document/internal/gap_buffer/BUILD.bazel")
        self.write("document/internal/gap_buffer/gap_buffer.h")
        self.write("document/internal/line_starts/BUILD.bazel")
        self.write("document/internal/line_starts/line_starts.h")

        self.assertEqual(self.findings(), ())

    def test_same_package_detail_include_is_allowed(self):
        self.write("editor/BUILD")
        self.write("editor/include/editor/editor.h",
                   '#include "editor/detail/inline_support.h"\n')
        self.write("editor/detail/inline_support.h")

        self.assertEqual(self.findings(), ())

    def test_nested_package_is_used_for_detail_ownership(self):
        self.write("editor/BUILD.bazel")
        self.write("editor/sub/BUILD.bazel")
        self.write("editor/sub/include/editor/sub/editor.h", (
            '#include "editor/sub/detail/local_support.h"\n'
            '#include "editor/detail/parent_support.h"\n'
        ))
        self.write("editor/sub/detail/local_support.h")
        self.write("editor/detail/parent_support.h")

        findings = self.findings()
        self.assertEqual(len(findings), 1, findings)
        self.assertEqual(findings[0][2], "editor/detail/parent_support.h")
        self.assertEqual(
            findings[0][3],
            "public header includes a detail header from another Bazel package",
        )

    def test_cross_component_internal_and_cross_package_detail_are_rejected(self):
        self.write("editor/BUILD.bazel")
        self.write("editor/include/editor/editor.h", (
            '#include "document/internal/buffer.h"\n'
            '#include "editor/sub/detail/support.h"\n'
        ))
        self.write("document/BUILD.bazel")
        self.write("document/internal/buffer.h")
        self.write("editor/sub/BUILD.bazel")
        self.write("editor/sub/detail/support.h")

        reasons = self.reasons()
        self.assertIn(
            "public header includes an internal header from another component",
            reasons,
        )
        self.assertIn(
            "public header includes a detail header from another Bazel package",
            reasons,
        )

    def test_impl_traversal_and_test_headers_are_rejected(self):
        self.write("editor/BUILD.bazel")
        self.write("editor/include/editor/editor.h", (
            '#include "editor/impl/private.h"\n'
            '#include "./editor/local.h"\n'
            '#include "../shared.h"\n'
            '#include "editor/tests/fixture.h"\n'
            '#include "editor/editor_tests.h"\n'
        ))
        self.write("editor/impl/private.h")
        self.write("editor/local.h")
        self.write("shared.h")
        self.write("editor/tests/fixture.h")
        self.write("editor/editor_tests.h")

        reasons = self.reasons()
        self.assertEqual(reasons.count("public header includes an impl path"), 1)
        self.assertEqual(
            reasons.count("public header include uses path traversal"), 2
        )
        self.assertEqual(reasons.count("public header includes a test header"), 2)

    def test_windows_style_separators_are_checked(self):
        self.write("editor/BUILD.bazel")
        self.write("editor/include/editor/editor.h", (
            '#include "editor\\\\impl\\\\private.h"\n'
            '#include ".\\\\editor\\\\local.h"\n'
        ))

        reasons = self.reasons()
        self.assertIn("public header includes an impl path", reasons)
        self.assertIn("public header include uses path traversal", reasons)

    def test_external_and_system_includes_are_ignored(self):
        self.write("editor/BUILD.bazel")
        self.write("editor/include/editor/editor.h", (
            "#include <vector>\n"
            "#include <third_party/detail/support.h>\n"
            '#include "unresolved/internal/header.h"\n'
        ))

        self.assertEqual(self.findings(), ())

    def test_ambiguous_private_include_is_reported_without_guessing(self):
        self.write("client/BUILD.bazel")
        self.write("client/include/client/api.h",
                   '#include "shared/internal/private.h"\n')
        self.write("first/include/shared/internal/private.h")
        self.write("second/include/shared/internal/private.h")

        findings = self.findings()
        self.assertEqual(len(findings), 1, findings)
        self.assertEqual(
            findings[0][3], "ambiguous private include spelling"
        )

    def test_comment_and_inactive_branch_scanning_is_best_effort(self):
        self.write("editor/include/editor/editor.h", (
            '// #include "comment/impl/ignored.h"\n'
            "/*\n"
            '#include "comment/impl/reported.h"\n'
            "*/\n"
            "#if 0\n"
            '#include "inactive/impl/reported.h"\n'
            "#endif\n"
        ))

        findings = self.findings()
        self.assertEqual(len(findings), 2, findings)
        self.assertEqual(
            [finding[2] for finding in findings],
            ["comment/impl/reported.h", "inactive/impl/reported.h"],
        )

    def test_macro_include_operands_are_ignored(self):
        self.write("editor/include/editor/editor.h", (
            "#define PRIVATE_HEADER \"editor/impl/private.h\"\n"
            "#include PRIVATE_HEADER\n"
        ))

        self.assertEqual(self.findings(), ())

    def test_symlinked_public_header_is_not_followed(self):
        self.write("editor/include/editor/target.h",
                   '#include "editor/impl/private.h"\n')
        link = self.root / "editor/include/editor/linked.h"
        try:
            link.symlink_to("target.h")
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"symlink creation is unavailable: {error}")

        inventory = checker.repository_inventory(self.root)
        self.assertNotIn(
            Path("editor/include/editor/linked.h"),
            [Path(header.relative.as_posix()) for header in inventory.headers],
        )
        findings = self.findings()
        self.assertEqual(len(findings), 1, findings)
        self.assertEqual(findings[0][0], "editor/include/editor/target.h")


if __name__ == "__main__":
    unittest.main()
