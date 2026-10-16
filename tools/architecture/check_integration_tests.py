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


if __name__ == "__main__":
    unittest.main()
