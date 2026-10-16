#!/usr/bin/env python3
"""Fixtures for the small structured loading-phase reader."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from discovery import Inventory
from facts import Location, OperationalError
from query import load_facts, parse_query


def rule(attributes=()):
    return {"type": "RULE", "rule": {
        "name": "//parcel:api", "ruleClass": "cc_library",
        "location": "/fixture/parcel/BUILD.bazel:7:11",
        "attribute": list(attributes),
        # These must never be interpreted as code dependencies.
        "ruleInput": ["//parcel:data", "//parcel:tool", "//:debug"],
    }}


class QueryParserTest(unittest.TestCase):
    def parse(self, *records):
        return parse_query("\n".join(json.dumps(r) for r in records), Path("/fixture"))

    def test_narrow_attributes_keep_sources_dependencies_grants_and_mappings(self):
        attributes = [
            {"name": name, "type": "LABEL_LIST", "stringListValue": [value],
             "explicitlySpecified": True}
            for name, value in (
                ("hdrs", "//parcel:parcel.h"), ("srcs", "//parcel:parcel.cpp"),
                ("textual_hdrs", "//parcel:detail.inc"),
                ("deps", "//address:api"), ("implementation_deps", "//storage:api"),
                ("data", "//parcel:data"), ("tools", "//parcel:tool"))]
        attributes += [
            {"name": "visibility", "type": "STRING_LIST",
             "stringListValue": ["//consumer:__pkg__"], "explicitlySpecified": False},
            {"name": "testonly", "type": "BOOLEAN", "booleanValue": True},
            {"name": "strip_include_prefix", "type": "STRING", "stringValue": "include"},
            {"name": "include_prefix", "type": "STRING", "stringValue": "postal"},
            {"name": "includes", "type": "STRING_LIST", "stringListValue": ["."]},
        ]
        target, = self.parse(rule(attributes)).targets
        self.assertEqual(target.label, "//parcel:api")
        self.assertEqual(target.kind, "cc_library")
        self.assertEqual(target.location, Location("parcel/BUILD.bazel", 7, 11))
        self.assertEqual(target.attribute("deps").value, ("//address:api",))
        self.assertEqual(target.attribute("implementation_deps").value, ("//storage:api",))
        self.assertEqual(target.attribute("hdrs").value, ("//parcel:parcel.h",))
        self.assertEqual(target.attribute("srcs").value, ("//parcel:parcel.cpp",))
        self.assertEqual(target.attribute("textual_hdrs").value, ("//parcel:detail.inc",))
        self.assertEqual(target.attribute("visibility").value, ("//consumer:__pkg__",))
        self.assertFalse(target.attribute("visibility").explicitly_specified)
        self.assertTrue(target.attribute("testonly").value)
        self.assertEqual(target.attribute("strip_include_prefix").value, "include")
        self.assertEqual(target.attribute("include_prefix").value, "postal")
        self.assertEqual(target.attribute("includes").value, (".",))
        self.assertIsNone(target.attribute("data"))
        self.assertIsNone(target.attribute("tools"))

    def test_selector_segments_branches_and_empty_values_stay_distinct(self):
        attribute = {
            "name": "deps", "type": "SELECTOR_LIST", "explicitlySpecified": True,
            "selectorList": {"type": "LABEL_LIST", "elements": [
                {"entries": [{"label": "//conditions:default",
                              "stringListValue": ["//base:api"]}], "hasDefaultValue": True},
                {"entries": [
                    {"label": "//:release", "isDefaultValue": True},
                    {"label": "//:debug", "stringListValue": ["//debug:api"]},
                    {"label": "//conditions:default"},
                ], "hasDefaultValue": True, "noMatchError": "choose a mode"},
            ]}}
        target, = self.parse(rule([attribute])).targets
        deps = target.attribute("deps")
        self.assertIsNone(deps.value)
        constant, conditional = deps.selectors
        self.assertEqual(constant.choices[0].value, ("//base:api",))
        self.assertEqual([c.condition for c in conditional.choices],
                         ["//:debug", "//:release", "//conditions:default"])
        self.assertEqual(conditional.choices[0].value, ("//debug:api",))
        self.assertIsNone(conditional.choices[1].value)
        self.assertTrue(conditional.choices[1].uses_attribute_default)
        self.assertEqual(conditional.choices[2].value, ())
        self.assertEqual(conditional.no_match_error, "choose a mode")

    def test_scalar_selector_is_retained(self):
        target, = self.parse(rule([{
            "name": "include_prefix", "type": "SELECTOR_LIST",
            "selectorList": {"type": "STRING", "elements": [{"entries": [
                {"label": "//conditions:default", "stringValue": "postal"}]}]},
        }])).targets
        self.assertEqual(target.attribute("include_prefix").selectors[0].choices[0].value,
                         "postal")

    def test_unsupported_attribute_and_selector_types_are_visible_limitations(self):
        target, = self.parse(rule([
            {"name": "includes", "type": "STRING_DICT"},
            {"name": "deps", "type": "SELECTOR_LIST",
             "selectorList": {"type": "LABEL_DICT_UNARY"}},
        ])).targets
        self.assertIn("unsupported attribute type", target.attribute("includes").limitation)
        self.assertIn("unsupported selector type", target.attribute("deps").limitation)

    def test_files_kinds_and_order_are_deterministic(self):
        records = [rule(),
                   {"type": "SOURCE_FILE", "sourceFile": {
                       "name": "//parcel:parcel.h",
                       "location": "/fixture/parcel/parcel.h:1:1"}},
                   {"type": "GENERATED_FILE", "generatedFile": {
                       "name": "//parcel:generated.h", "generatingRule": "//parcel:generate",
                       "location": "/fixture/parcel/BUILD.bazel:10:8"}},
                   {"type": "PACKAGE_GROUP", "packageGroup": {"name": "//:friends"}}]
        self.assertEqual(self.parse(*records), self.parse(*reversed(records)))
        self.assertEqual([f.label for f in self.parse(*records).files],
                         ["//parcel:generated.h", "//parcel:parcel.h"])
        self.assertEqual(self.parse(*records).files[0].generated_by, "//parcel:generate")

    def test_invalid_output_is_an_operational_error(self):
        for output in ("not JSON", "{}", json.dumps(rule() | {"rule": {}})):
            with self.subTest(output=output), self.assertRaises(OperationalError):
                parse_query(output, Path("/fixture"))
        with self.assertRaisesRegex(OperationalError, "duplicate target"):
            self.parse(rule(), rule())

    def test_unrecognized_records_are_coverage_limits_alongside_usable_facts(self):
        facts = self.parse({"type": "FUTURE_TARGET"}, rule())
        self.assertEqual(len(facts.targets), 1)
        self.assertEqual(facts.limitations[0].location, Location("."))
        self.assertIn("FUTURE_TARGET", facts.limitations[0].reason)


class QueryInvocationTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / ".bazelversion").write_text("9.2.0\n", encoding="utf-8")
        self.inventory = Inventory((), ("BUILD.bazel", "mail room/BUILD", "parcel/BUILD"),
                                   ("", "mail room", "parcel"))

    def test_explicit_packages_pin_and_selector_options(self):
        record = rule()
        record["rule"]["location"] = str(self.root / "parcel/BUILD") + ":7:11"
        with patch("query.subprocess.run", side_effect=[
                subprocess.CompletedProcess([], 0, "Bazelisk v1\nbazel 9.2.0\n", ""),
                subprocess.CompletedProcess([], 0, json.dumps(record), "")]) as run:
            facts = load_facts(self.root, self.inventory)
        self.assertEqual(len(facts.targets), 1)
        command = run.call_args.args[0]
        self.assertEqual(command, ["bazel", "query", 'set("//:*" "//mail room:*" "//parcel:*")',
                                   "--output=streamed_jsonproto", "--proto:flatten_selects=false"])
        self.assertEqual(run.call_args.kwargs["env"]["USE_BAZEL_VERSION"], "9.2.0")
        self.assertEqual(run.call_args.kwargs["cwd"], self.root)

    def test_bazel_failure_does_not_return_partial_facts(self):
        with patch("query.subprocess.run", side_effect=[
                subprocess.CompletedProcess([], 0, "bazel 9.2.0\n", ""),
                subprocess.CompletedProcess([], 7, json.dumps(rule()), "package load failed")]):
            with self.assertRaisesRegex(OperationalError, "Bazel exited 7: package load failed"):
                load_facts(self.root, self.inventory)

    def test_missing_bazel_and_wrong_version_fail(self):
        with patch("query.subprocess.run", side_effect=FileNotFoundError("bazel")):
            with self.assertRaisesRegex(OperationalError, "cannot run Bazel"):
                load_facts(self.root, self.inventory)
        with patch("query.subprocess.run", return_value=subprocess.CompletedProcess(
                [], 0, "bazel 8.0.0\n", "")):
            with self.assertRaisesRegex(OperationalError, "expected Bazel 9.2.0"):
                load_facts(self.root, self.inventory)

    def test_missing_pin_and_successful_empty_query_fail(self):
        with patch("query.subprocess.run", side_effect=[
                subprocess.CompletedProcess([], 0, "bazel 9.2.0\n", ""),
                subprocess.CompletedProcess([], 0, "", "")]):
            with self.assertRaisesRegex(OperationalError, "returned no facts"):
                load_facts(self.root, self.inventory)
        (self.root / ".bazelversion").unlink()
        with self.assertRaisesRegex(OperationalError, "cannot read .bazelversion"):
            load_facts(self.root, self.inventory)


if __name__ == "__main__":
    unittest.main()
