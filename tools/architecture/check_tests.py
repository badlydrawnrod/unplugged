#!/usr/bin/env python3
"""Exercise command status, reporting, and invocation independently of Bazel."""

from contextlib import redirect_stderr, redirect_stdout
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import check
from discovery import Inventory
from facts import BuildFacts, Limitation, Location, OperationalError
from report import Report


class CommandTest(unittest.TestCase):
    def invoke(self, report=None, error=None):
        stdout, stderr = io.StringIO(), io.StringIO()
        inventory = Inventory((), ("BUILD.bazel",), ("",))
        with patch("check.discover_files", return_value=inventory), \
                patch("check.load_facts", return_value=BuildFacts((), ()), side_effect=error), \
                patch("check.audit", return_value=report), \
                redirect_stdout(stdout), redirect_stderr(stderr):
            status = check.main([])
        return status, stdout.getvalue(), stderr.getvalue()

    def test_passing_and_limited_reports_exit_zero(self):
        report = Report()
        report.checked("includes")
        report.skip("includes", Location("parcel/parcel.h", 4), "computed include")
        status, stdout, stderr = self.invoke(report)
        self.assertEqual(status, 0)
        self.assertIn("Not checked: 1", stdout)
        self.assertIn("Violations: 0", stdout)
        self.assertFalse(stderr)

    def test_definite_violation_fails_alongside_limitations(self):
        report = Report()
        report.skip("includes", Location("parcel/parcel.h", 4), "computed include")
        report.violation("entry-targets", Location("client/BUILD", 7), "API bypass")
        status, stdout, stderr = self.invoke(report)
        self.assertEqual(status, 1)
        self.assertIn("Violations: 1", stdout)
        self.assertIn("Not checked: 1", stdout)
        self.assertFalse(stderr)

    def test_operational_error_has_distinct_status_without_success_report(self):
        status, stdout, stderr = self.invoke(error=OperationalError("query failed"))
        self.assertEqual(status, 2)
        self.assertFalse(stdout)
        self.assertEqual(stderr, "Operational error: query failed\n")

    def test_default_repository_does_not_depend_on_current_directory(self):
        original = Path.cwd()
        self.addCleanup(os.chdir, original)
        with tempfile.TemporaryDirectory() as outside:
            os.chdir(outside)
            with patch("check.discover_files", side_effect=OperationalError("stop")) as discover, \
                    redirect_stderr(io.StringIO()):
                self.assertEqual(check.main([]), 2)
            discover.assert_called_once_with(check.REPO_ROOT)

    def test_explicit_relative_repository_is_resolved(self):
        with patch("check.discover_files", side_effect=OperationalError("stop")) as discover, \
                redirect_stderr(io.StringIO()):
            self.assertEqual(check.main(["--repo", "parcel"]), 2)
        discover.assert_called_once_with(Path("parcel").resolve())

    def test_unknown_facts_and_absent_boundaries_are_visible(self):
        facts = BuildFacts((), (), (Limitation(Location("."), "unsupported query fact"),))
        report = check.audit(Path.cwd(), Inventory((), (), ()), facts)
        self.assertFalse(report.failed)
        for rule in check.RULES:
            self.assertTrue(any(f.rule == rule and f.reason == "unsupported query fact"
                                for f in report.findings))
            self.assertTrue(any(f.rule == rule and "no supported" in f.reason
                                for f in report.findings))


if __name__ == "__main__":
    unittest.main()
