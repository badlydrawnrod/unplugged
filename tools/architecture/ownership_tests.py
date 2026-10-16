#!/usr/bin/env python3
"""Tests for shared ownership and conservative access evidence."""

import unittest

from discovery import Inventory
from facts import Attribute, BuildFacts, Choice, Location, Selector, Target
from ownership import (Ownership, explicit_grant, label_path, strings,
                       test_context)
from report import Report


def target(label, kind="cc_library", **values):
    return Target(label, kind, Location(label[2:].split(":")[0] + "/BUILD", 4),
                  tuple(Attribute(name, "BOOLEAN" if isinstance(value, bool)
                                  else "STRING" if isinstance(value, str) else "LABEL_LIST",
                                  True, value=value) for name, value in values.items()))


class OwnershipTest(unittest.TestCase):
    def ownership(self, *targets):
        return Ownership(BuildFacts(targets, ()), Inventory((), (), (
            "", "parcel", "parcel/tests", "parcel/internal/ring", "parcel/internal/ring/tests",
            "address")))

    def test_tests_and_implementation_inherit_nearest_boundary(self):
        outer = target("//parcel:api", hdrs=("//parcel:include/parcel/parcel.h",))
        inner = target("//parcel/internal/ring:api", hdrs=("//parcel/internal/ring:ring.h",))
        implementation = target("//parcel/internal/ring:storage")
        tests = target("//parcel/internal/ring/tests:tests", "cc_test")
        ownership = self.ownership(outer, inner, implementation, tests)
        self.assertEqual(ownership.owner_path("parcel/tests/read_tests.cpp"), "parcel")
        self.assertEqual(ownership.owner_target(implementation), "parcel/internal/ring")
        self.assertEqual(ownership.owner_target(tests), "parcel/internal/ring")
        self.assertEqual(ownership.package_for_path("parcel/tests/read_tests.cpp"), "parcel/tests")
        self.assertEqual(ownership.owner_path("address/address.h"), None)

    def test_conflicting_header_boundary_is_uncertain(self):
        outer = target("//parcel:api", hdrs=("//address:address.h",))
        other = target("//address:api", hdrs=("//address:address.h",))
        self.assertIsNone(self.ownership(outer, other).owner_path("address/address.h"))

    def test_unrecognized_api_boundary_does_not_inherit_parent_ownership(self):
        ownership = self.ownership(target("//parcel:api"),
                                   target("//parcel/internal/ring:api", "alias"))
        self.assertIsNone(ownership.owner_path("parcel/internal/ring/tests/read.cpp"))

    def test_api_variants_require_same_package_headers_and_mappings(self):
        api = target("//parcel:api", hdrs=("//parcel:parcel.h",), include_prefix="postal")
        variant = target("//parcel:debug_api", hdrs=("//parcel:parcel.h",), include_prefix="postal")
        helper = target("//parcel:storage", hdrs=("//parcel:impl/storage.h",))
        moved = target("//parcel:remapped", hdrs=("//parcel:parcel.h",), include_prefix="other")
        ownership = self.ownership(api, variant, helper, moved)
        self.assertTrue(ownership.entry_status(api))
        self.assertTrue(ownership.entry_status(variant))
        self.assertFalse(ownership.entry_status(helper))
        self.assertFalse(ownership.entry_status(moved))

    def test_external_and_nonlocal_labels_are_not_local_paths(self):
        self.assertEqual(label_path("//parcel:include/parcel/parcel.h"), "parcel/include/parcel/parcel.h")
        self.assertEqual(label_path("//:root.h"), "root.h")
        self.assertIsNone(label_path("@other//parcel:parcel.h"))
        self.assertIsNone(label_path("//:../escaped.h"))


class EvidenceTest(unittest.TestCase):
    def test_only_explicit_finite_exact_package_grants_are_exceptions(self):
        for values, expected in (
                (("//client:__pkg__",), True),
                (("//other:__pkg__",), False),
                (("//visibility:private",), False),
                (("//visibility:public",), False),
                (("//visibility:public", "//client:__pkg__"), False),
                (("//client:__subpackages__",), None),
                (("//:friends",), None)):
            with self.subTest(values=values):
                self.assertEqual(explicit_grant(target("//parcel:impl", visibility=values),
                                                "client"), expected)
        inherited = Target("//parcel:impl", "cc_library", Location("parcel/BUILD"), (
            Attribute("visibility", "STRING_LIST", False, value=("//client:__pkg__",)),))
        self.assertIsNone(explicit_grant(inherited, "client"))

    def test_literals_and_conditional_references_remain_distinct(self):
        deps = Attribute("deps", "LABEL_LIST", True, selectors=(
            Selector((Choice("//conditions:default", ("//base:api",)),), True, ""),
            Selector((Choice("//:debug", ("//debug:api", "//base:api")),
                      Choice("//conditions:default", ())), True, "")))
        sample = Target("//parcel:api", "cc_library", Location("parcel/BUILD"), (deps,))
        refs, gaps = strings(sample, "deps")
        self.assertEqual([(r.value, r.conditional) for r in refs],
                         [("//base:api", False), ("//debug:api", True)])
        self.assertFalse(gaps)

    def test_unknown_defaults_and_types_are_visible(self):
        defaulted = Attribute("deps", "LABEL_LIST", True, selectors=(
            Selector((Choice("//conditions:default", None, True),), True, ""),))
        sample = Target("//parcel:api", "cc_library", Location("parcel/BUILD"), (defaulted,))
        refs, gaps = strings(sample, "deps")
        self.assertFalse(refs)
        self.assertTrue(gaps)

    def test_test_context_is_not_guessed_from_a_name(self):
        self.assertFalse(test_context(target("//parcel:tests")))
        self.assertTrue(test_context(target("//parcel:unit", "cc_test")))
        self.assertTrue(test_context(target("//parcel:helper", testonly=True)))
        uncertain = target("//parcel:helper", testonly="computed")
        self.assertIsNone(test_context(uncertain))

    def test_report_sorts_and_separates_limitations_from_violations(self):
        report = Report()
        report.skip("includes", Location("z.h", 2), "computed include")
        report.violation("cycles", Location("a/BUILD", 3), "a -> b -> a")
        report.violation("entry-targets", Location("a/BUILD", 3), "bypass")
        report.checked("includes")
        self.assertTrue(report.failed)
        self.assertEqual([f.rule for f in report.ordered(not_checked=False)],
                         ["cycles", "entry-targets"])
        self.assertIn("Not checked: 1", report.render())
        self.assertIn("includes: 1 / 1", report.render())


if __name__ == "__main__":
    unittest.main()
