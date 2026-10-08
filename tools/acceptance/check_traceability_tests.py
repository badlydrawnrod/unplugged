#!/usr/bin/env python3
"""Focused traceability checks using isolated temporary repositories."""

from contextlib import redirect_stderr
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import check_traceability as checker


FEATURE = "Feature: Editing\n  Description of the behavior.\n\n  Scenario: Preserves content\n    Given a document\n    Then its content is preserved\n"
BINDING = "// Feature: component/features/editing.feature\n// Scenario: Preserves content\nTEST(EditorTest, PreservesContent) {}\n"


class TraceabilityTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.write("component/features/editing.feature", FEATURE)
        self.write("component/editor_tests.cpp", BINDING)

    def write(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")

    def report(self):
        # These fixtures contain no generated output; discovery is tested below.
        paths = sorted(path.relative_to(self.root) for path in self.root.rglob("*")
                       if path.is_file())
        return checker.check_files(self.root, paths)

    def reject(self, expected):
        errors = self.report().errors
        self.assertTrue(any(expected in error for error in errors), errors)
        self.assertTrue(all(":" in error.split(": ")[0] for error in errors), errors)
        return errors

    def test_success(self):
        report = self.report()
        self.assertEqual(report.errors, [])
        self.assertEqual(len(report.scenarios), 1)
        self.assertEqual(sum(map(len, report.bindings.values())), 1)

    def test_supported_test_forms_and_multiline_definitions(self):
        for macro in ("TEST", "TEST_F", "TEST_P"):
            with self.subTest(macro=macro):
                self.write("component/editor_tests.cpp", BINDING.replace(
                    "TEST(EditorTest, PreservesContent) {}",
                    f"{macro}(\n    EditorTest,\n    PreservesContent)\n{{}}"))
                self.assertEqual(self.report().errors, [])

    def test_multiple_bindings_are_allowed(self):
        self.write("component/other_tests.cpp", BINDING.replace(
            "EditorTest", "OtherTest"))
        report = self.report()
        self.assertEqual(report.errors, [])
        self.assertEqual(sum(map(len, report.bindings.values())), 2)

    def test_same_title_in_distinct_features_is_allowed(self):
        self.write("other/features/editing.feature", FEATURE)
        self.write("other/editor_tests.cpp", BINDING.replace("component/", "other/"))
        self.assertEqual(self.report().errors, [])

    def test_unbound_scenario_reports_feature_location(self):
        self.write("component/editor_tests.cpp", "TEST(EditorTest, Ordinary) {}")
        errors = self.reject("unbound scenario")
        self.assertIn("component/features/editing.feature:4", errors[0])

    def test_nonexistent_feature_reports_test_location(self):
        self.write("component/editor_tests.cpp", BINDING.replace("editing.feature", "missing.feature"))
        errors = self.reject("nonexistent feature reference")
        self.assertIn("component/editor_tests.cpp:1", errors[0])

    def test_nonexistent_scenario_reports_both_paths(self):
        self.write("component/editor_tests.cpp", BINDING.replace("Preserves content", "Wrong title"))
        errors = self.reject("nonexistent scenario reference")
        self.assertIn("component/editor_tests.cpp:2", errors[0])
        self.assertIn("component/features/editing.feature", errors[0])

    def test_titles_are_exact(self):
        self.write("component/editor_tests.cpp", BINDING.replace("Preserves content", "preserves content"))
        self.reject("nonexistent scenario reference")

    def test_duplicate_scenario_reports_both_locations(self):
        self.write("component/features/editing.feature", FEATURE +
                   "\n  Scenario: Preserves content\n    Given another document\n")
        errors = self.reject("duplicate scenario identity")
        self.assertIn("component/features/editing.feature:8", errors[0])
        self.assertIn("component/features/editing.feature:4", errors[0])

    def test_malformed_or_detached_annotations(self):
        malformed = [
            "int unrelated; " + BINDING,
            BINDING.replace("// Scenario:", "int unrelated; // Scenario:"),
            BINDING.replace("// Feature:", "//Feature:"),
            BINDING.replace("// Feature:", "// Feature"),
            BINDING.replace("component/features/editing.feature", ""),
            BINDING.replace("Preserves content\n", "\n"),
            BINDING.replace("// Scenario: Preserves content\n", ""),
            BINDING.replace("// Feature: component/features/editing.feature\n", ""),
            BINDING.replace("// Scenario", "\n// Scenario"),
        ]
        detached = [
            BINDING.replace("TEST(", "\nTEST("),
            BINDING.replace("TEST(", "// Comment\nTEST("),
            BINDING.replace("TEST(", "int unrelated;\nTEST("),
            BINDING.replace("TEST(", "CUSTOM_TEST("),
            BINDING.replace(" {}", ";"),
            BINDING.replace("TEST(EditorTest, PreservesContent) {}\n", ""),
            BINDING.replace("TEST(", "// TEST("),
        ]
        for value in malformed + detached:
            with self.subTest(value=value):
                self.write("component/editor_tests.cpp", value)
                self.reject("annotations")

    def test_invalid_feature_paths(self):
        for path in ("/component/features/editing.feature", "../editing.feature",
                     "./component/features/editing.feature", "component//features/editing.feature",
                     "component\\features\\editing.feature", "component/features/editing.txt"):
            with self.subTest(path=path):
                self.write("component/editor_tests.cpp", BINDING.replace("component/features/editing.feature", path))
                self.reject("repository-relative .feature path")

    def test_comments_and_literals_do_not_supply_bindings(self):
        for value in ("/*\n" + BINDING + "*/", 'const char* text = R"tag(\n' + BINDING + ')tag";',
                      '// ' + BINDING.replace("\n", "\n// "),
                      'const char* text = "TEST(EditorTest, PreservesContent) {}";'):
            with self.subTest(value=value):
                self.write("component/editor_tests.cpp", value)
                self.reject("unbound scenario")

    def test_supported_feature_comments_and_star_steps(self):
        self.write("component/features/editing.feature", FEATURE.replace(
            "Given a document", "* a document").replace(
            "  Scenario:", "  # Behavioral contract\n  Scenario:"))
        self.assertEqual(self.report().errors, [])

    def test_ordinary_tests_do_not_require_annotations(self):
        self.write("component/editor_tests.cpp", BINDING + "TEST(Ordinary, Unannotated) {}\n")
        self.assertEqual(self.report().errors, [])

    def test_unsupported_gherkin_is_rejected(self):
        for value in ("Background: Setup", "Rule: Editing", "Scenario Outline: Examples",
                      "Scenario Template: Examples", "Examples: Values", "Example: Editing",
                      "@tag", "| name | value |", '"""', "```", "# language: fr", "#language: fr"):
            with self.subTest(value=value):
                self.write("component/features/editing.feature", FEATURE + value + "\n")
                self.reject("unsupported Gherkin")

    def test_malformed_gherkin_is_rejected(self):
        for value in ("", "Feature: Editing\n", "Scenario: Missing feature\n Given a document\n",
                      "Feature:\n Scenario: Content\n Given a document\n",
                      "Feature: Editing\n Scenario:\n Given a document\n",
                      "Feature: Editing\n Scenario: No steps\n",
                      FEATURE + "Feature: Second feature\n",
                      "Feature: Editing\n Given a document\n" + FEATURE,
                      FEATURE + "Unexpected content\n",
                      "Feature: Editing\n Scenario Missing colon\n" + FEATURE):
            with self.subTest(value=value):
                self.write("component/features/editing.feature", value)
                self.assertTrue(self.report().errors)

    def test_no_features_is_rejected(self):
        (self.root / "component/features/editing.feature").unlink()
        self.reject("no feature files found")

    def test_discovery_covers_moves_and_new_files_without_following_output(self):
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "."], check=True)
        old = self.root / "component/editor_tests.cpp"
        old.rename(self.root / "component/moved_tests.cpp")
        self.write(".gitignore", "/build/\n/bazel-*\n")
        self.write("build/generated.feature", "invalid output")
        self.write("bazel-output/generated.cpp", "invalid output")
        (self.root / "output-link").symlink_to(self.root / "build", target_is_directory=True)
        paths = checker.discover_files(self.root)
        self.assertEqual(paths, [Path("component/features/editing.feature"), Path("component/moved_tests.cpp")])
        self.assertEqual(checker.check_files(self.root, paths).errors, [])

    def test_main_reports_failure_with_path_and_line(self):
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        self.write("component/editor_tests.cpp", BINDING.replace(
            "Preserves content", "Wrong title"))
        errors = io.StringIO()
        with patch.object(checker, "REPO_ROOT", self.root), \
                patch.object(sys, "argv", ["check_traceability.py"]), \
                redirect_stderr(errors):
            self.assertEqual(checker.main(), 1)
        self.assertIn("component/editor_tests.cpp:2", errors.getvalue())
        self.assertIn("nonexistent scenario reference", errors.getvalue())
        self.assertIn("component/features/editing.feature:4", errors.getvalue())

    def test_cli_runs_from_another_directory(self):
        result = subprocess.run([sys.executable, str(Path(checker.__file__).resolve())],
                                cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Traceability passed:", result.stdout)


if __name__ == "__main__":
    unittest.main()
