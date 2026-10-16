#!/usr/bin/env python3
"""Confirm the reader against the pinned Bazel, with no C++ toolchain analysis."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from discovery import discover_files
from facts import OperationalError
from query import load_facts


DEFINITIONS = '''\
def _impl(ctx):
    return []

sample = rule(implementation = _impl, attrs = {
    "srcs": attr.label_list(allow_files = True),
    "hdrs": attr.label_list(allow_files = True),
    "deps": attr.label_list(),
    "implementation_deps": attr.label_list(),
    "data": attr.label_list(allow_files = True),
    "tools": attr.label_list(),
    "include_prefix": attr.string(),
    "strip_include_prefix": attr.string(),
    "includes": attr.string_list(),
})
'''

BUILD = '''\
load(":defs.bzl", "sample")
package(default_visibility = ["//client:__pkg__"])
config_setting(name = "debug", values = {"compilation_mode": "dbg"})
constraint_setting(name = "platform_kind")
constraint_value(name = "unavailable", constraint_setting = ":platform_kind")
sample(name = "base")
sample(name = "left")
sample(name = "right")
sample(name = "hidden")
sample(
    name = "api",
    srcs = ["parcel.cpp"],
    hdrs = ["parcel.h", ":generated.h"] + select({
        ":debug": ["debug.h"], "//conditions:default": [],
    }),
    deps = [":base"] + select({":debug": [":left"], "//conditions:default": [":right"]}),
    implementation_deps = [":hidden"],
    include_prefix = "postal",
    strip_include_prefix = "include",
    includes = ["extras"],
    data = ["payload.txt"],
    tools = [":tool"],
)
sample(name = "tool")
sample(name = "test_api", testonly = True, hdrs = ["parcel.h"],
       visibility = ["//client/tests:__pkg__"])
sample(name = "manual_incompatible", tags = ["manual"],
       target_compatible_with = [":unavailable"])
genrule(name = "generate", outs = ["generated.h"], cmd = "touch $@")
alias(name = "entry", actual = ":api")
'''


class RealBazelTest(unittest.TestCase):
    def test_loading_phase_selectors_and_failure_propagation(self):
        with tempfile.TemporaryDirectory(prefix="architecture-query-") as temporary:
            root = Path(temporary) / "repo"
            root.mkdir()
            subprocess.run(["git", "init", "--quiet"], cwd=root, check=True,
                           capture_output=True)
            (root / "MODULE.bazel").write_text('module(name = "fixture")\n', encoding="utf-8")
            pin = Path(__file__).resolve().parents[2] / ".bazelversion"
            (root / ".bazelversion").write_bytes(pin.read_bytes())
            (root / "BUILD.bazel").write_text(BUILD, encoding="utf-8")
            (root / "defs.bzl").write_text(DEFINITIONS, encoding="utf-8")
            (root / ".gitignore").write_text("ignored/\nbazel-*\n", encoding="utf-8")
            for name in ("parcel.cpp", "parcel.h", "debug.h", "payload.txt"):
                (root / name).touch()
            package = root / "mail room"
            package.mkdir()
            (package / "BUILD.bazel").write_text(
                'load("//:defs.bzl", "sample")\nsample(name = "api")\n', encoding="utf-8")
            ignored = root / "ignored"
            ignored.mkdir()
            (ignored / "BUILD.bazel").write_text("invalid ignored BUILD\n", encoding="utf-8")
            # Separate output root, batch mode, and no user rc keep the fixture
            # independent of the working repository and leave no running server.
            bazel = ("bazel", "--batch", "--ignore_all_rc_files",
                     f"--output_user_root={temporary}/bazel")
            inventory = discover_files(root)
            self.assertEqual(inventory.packages, ("", "mail room"))
            facts = load_facts(root, inventory, bazel=bazel)
            self.assertEqual(facts, load_facts(root, inventory, bazel=bazel))
            targets = {t.label: t for t in facts.targets}
            self.assertIn("//mail room:api", targets)
            api = targets["//:api"]
            self.assertEqual(api.kind, "sample")
            self.assertEqual(api.location.path, "BUILD.bazel")
            self.assertEqual(api.attribute("srcs").value, ("//:parcel.cpp",))
            self.assertEqual(api.attribute("implementation_deps").value, ("//:hidden",))
            constant, conditional = api.attribute("deps").selectors
            self.assertEqual(constant.choices[0].value, ("//:base",))
            self.assertEqual({c.condition: c.value for c in conditional.choices},
                             {"//:debug": ("//:left",),
                              "//conditions:default": ("//:right",)})
            self.assertEqual(api.attribute("hdrs").selectors[1].choices[1].value, ())
            self.assertEqual(api.attribute("visibility").value, ("//client:__pkg__",))
            self.assertFalse(api.attribute("visibility").explicitly_specified)
            self.assertEqual(api.attribute("includes").value, ("extras",))
            self.assertEqual(api.attribute("include_prefix").value, "postal")
            self.assertEqual(api.attribute("strip_include_prefix").value, "include")
            self.assertIsNone(api.attribute("data"))
            self.assertIsNone(api.attribute("tools"))
            self.assertTrue(targets["//:test_api"].attribute("testonly").value)
            self.assertEqual(targets["//:test_api"].attribute("visibility").value,
                             ("//client/tests:__pkg__",))
            self.assertIn("//:manual_incompatible", targets)
            self.assertEqual(targets["//:entry"].attribute("actual").value, "//:api")
            generated, = [f for f in facts.files if f.label == "//:generated.h"]
            self.assertEqual(generated.generated_by, "//:generate")

            (root / "BUILD.bazel").write_text("invalid BUILD syntax !!!\n", encoding="utf-8")
            with self.assertRaisesRegex(OperationalError, "Bazel exited"):
                load_facts(root, inventory, bazel=bazel)


if __name__ == "__main__":
    unittest.main()
