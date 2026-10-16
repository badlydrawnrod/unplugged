#!/usr/bin/env python3
"""Exercise all rules together using actual Git discovery and Bazel facts."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from check import audit
from discovery import discover_files
from facts import BuildFacts
from query import load_facts
from report import RULES


class PipelineTest(unittest.TestCase):
    def _write(self, root, files):
        for name, content in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")

    def _load_audit(self, root, directory):
        inventory = discover_files(root)
        facts = load_facts(root, inventory, bazel=(
            "bazel", "--batch", "--ignore_all_rc_files",
            f"--output_user_root={directory}/bazel"))
        return inventory, facts, audit(root, inventory, facts)

    def test_all_rules_and_limitations_survive_the_real_fact_pipeline(self):
        with tempfile.TemporaryDirectory(prefix="architecture-check-") as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            subprocess.run(["git", "init", "--quiet"], cwd=root, check=True,
                           capture_output=True)
            files = {
                "MODULE.bazel": 'module(name = "architecture_fixture")\n',
                ".gitignore": "bazel-*\n",
                # Loading-phase fixtures need C++-shaped declarations, without
                # analyzing actions or acquiring a compiler/toolchain.
                "defs.bzl": '''\
def _impl(ctx):
    return []

cc_library = rule(implementation = _impl, attrs = {
    "hdrs": attr.label_list(allow_files = True),
    "deps": attr.label_list(),
})
''',
                "BUILD.bazel": "",
                "orchard/BUILD.bazel": '''\
load("//:defs.bzl", "cc_library")
cc_library(name = "api", hdrs = ["orchard.h"], deps = ["//basket:api"])
cc_library(name = "storage", hdrs = ["impl/storage.h"])
''',
                "basket/BUILD.bazel": '''\
load("//:defs.bzl", "cc_library")
cc_library(name = "api", hdrs = ["basket.h"], deps = ["//orchard:storage"])
''',
                "orchard/internal/cache/BUILD.bazel": '''\
load("//:defs.bzl", "cc_library")
cc_library(name = "api", hdrs = ["cache.h"])
''',
                "orchard/orchard.h": '#include "basket/basket.h"\n',
                "basket/basket.h": '#include HEADER\n#include "orchard/impl/storage.h"\n'
                                   '#include "orchard/internal/cache/cache.h"\n',
                "orchard/impl/storage.h": "",
                "orchard/internal/cache/cache.h": "",
            }
            for name, content in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            pin = Path(__file__).resolve().parents[2] / ".bazelversion"
            (root / ".bazelversion").write_bytes(pin.read_bytes())
            inventory = discover_files(root)
            facts = load_facts(root, inventory, bazel=(
                "bazel", "--batch", "--ignore_all_rc_files",
                f"--output_user_root={directory}/bazel"))
            report = audit(root, inventory, facts)
            self.assertTrue(report.failed, report.render())
            self.assertEqual({f.rule for f in report.ordered(not_checked=False)}, set(RULES))
            self.assertTrue(any("computed" in f.reason for f in report.ordered(not_checked=True)))
            for rule in RULES:
                self.assertGreater(report.counts[rule], 0)
            reordered = BuildFacts(tuple(reversed(facts.targets)), tuple(reversed(facts.files)))
            self.assertEqual(report.render(), audit(root, inventory, reordered).render())

    def test_new_component_is_discovered_and_enforced_without_registry_edits(self):
        with tempfile.TemporaryDirectory(prefix="architecture-discovery-") as directory:
            root = Path(directory) / "repo"
            root.mkdir()
            subprocess.run(["git", "init", "--quiet"], cwd=root, check=True,
                           capture_output=True)
            files = {
                "MODULE.bazel": 'module(name = "architecture_discovery_fixture")\n',
                ".gitignore": "bazel-*\n",
                "defs.bzl": '''\
def _impl(ctx):
    return []

cc_library = rule(implementation = _impl, attrs = {
    "hdrs": attr.label_list(allow_files = True),
    "deps": attr.label_list(),
})
''',
                "BUILD.bazel": "",
                "willow/BUILD.bazel": '''\
load("//:defs.bzl", "cc_library")
cc_library(name = "api", hdrs = ["willow.h"],
           deps = ["//willow/internal/ring:api"])
''',
                "willow/internal/ring/BUILD.bazel": '''\
load("//:defs.bzl", "cc_library")
cc_library(name = "api", hdrs = ["ring.h"])
''',
                "willow/willow.h": '#include "internal/ring/ring.h"\n',
                "willow/internal/ring/ring.h": "",
            }
            self._write(root, files)
            pin = Path(__file__).resolve().parents[2] / ".bazelversion"
            (root / ".bazelversion").write_bytes(pin.read_bytes())

            inventory, facts, report = self._load_audit(root, directory)
            self.assertEqual(inventory.packages,
                             ("", "willow", "willow/internal/ring"))
            self.assertFalse(report.failed, report.render())
            self.assertFalse(report.ordered(not_checked=True), report.render())
            for rule in RULES:
                self.assertGreater(report.counts[rule], 0, report.render())

            # These files are untracked but active: Git discovery and the next
            # explicit Bazel package query must pick up the new component.
            self._write(root, {
                "cedar/BUILD.bazel": '''\
load("//:defs.bzl", "cc_library")
cc_library(name = "api", hdrs = ["cedar.h"])
''',
                "cedar/cedar.h": '#include "willow/internal/ring/ring.h"\n'
                                  '#include HEADER\n',
            })
            inventory, facts, report = self._load_audit(root, directory)
            self.assertIn("cedar", inventory.packages)
            self.assertIn("//cedar:api", {target.label for target in facts.targets})
            violations = report.ordered(not_checked=False)
            self.assertTrue(report.failed, report.render())
            self.assertTrue(any(f.rule == "private-access"
                                and f.location.path == "cedar/cedar.h"
                                and "willow/internal/ring/ring.h" in f.reason
                                for f in violations), report.render())
            self.assertTrue(any("computed" in f.reason
                                for f in report.ordered(not_checked=True)), report.render())

            # Repair the definite private include. The computed include remains
            # visible as a limitation, and repeated real loads stay deterministic.
            (root / "cedar/cedar.h").write_text(
                '#include "willow/willow.h"\n#include HEADER\n', encoding="utf-8")
            inventory, facts, limited = self._load_audit(root, directory)
            self.assertFalse(limited.failed, limited.render())
            self.assertFalse(limited.ordered(not_checked=False), limited.render())
            self.assertTrue(any("computed" in f.reason
                                for f in limited.ordered(not_checked=True)), limited.render())

            repeated_inventory, repeated_facts, repeated = self._load_audit(root, directory)
            self.assertEqual(inventory, repeated_inventory)
            self.assertEqual(facts, repeated_facts)
            self.assertEqual(limited.render(), repeated.render())


if __name__ == "__main__":
    unittest.main()
