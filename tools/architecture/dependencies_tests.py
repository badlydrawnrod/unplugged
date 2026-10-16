"""Focused dependency-policy fixtures independent of a C++ toolchain."""

from types import SimpleNamespace
import unittest

from dependencies import check_dependencies
from facts import Attribute, BuildFacts, Choice, Location, Selector, Target
from ownership import Ownership
from report import Report


def attribute(name, value, *, explicit=True):
    return Attribute(name, "BOOLEAN" if isinstance(value, bool) else "LABEL_LIST",
                     explicit, value)


def target(label, *, kind="cc_library", deps=(), implementation_deps=(),
           hdrs=(), visibility=None, testonly=False, selectors=()):
    attrs = [attribute("hdrs", hdrs), attribute("testonly", testonly)]
    attrs.append(Attribute("deps", "LABEL_LIST", True, deps, selectors))
    attrs.append(attribute("implementation_deps", implementation_deps))
    if visibility is not None:
        attrs.append(attribute("visibility", visibility))
    package = label[2:].split(":")[0]
    return Target(label, kind, Location(f"{package}/BUILD.bazel"), tuple(attrs))


def run(*targets):
    facts = BuildFacts(targets, ())
    inventory = SimpleNamespace(packages=tuple(sorted({
        t.label[2:].split(":")[0] for t in targets})))
    report = Report()
    check_dependencies(facts, Ownership(facts, inventory), report)
    return report


def api(package, **kwargs):
    return target(f"//{package}:api", hdrs=(f"//{package}:public.h",), **kwargs)


class DependenciesTests(unittest.TestCase):
    def test_entry_targets_variants_and_inherited_owner(self):
        report = run(api("canvas"), api("ink"),
                     target("//ink:debug_api", hdrs=("//ink:public.h",)),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//canvas:private",)),
                     target("//canvas:private"),
                     target("//app:main", kind="cc_binary", deps=("//ink:debug_api", "//canvas:api")))
        self.assertFalse(report.failed, report.render())
        self.assertEqual([f.rule for f in report.ordered(not_checked=True)], ["cycles", "cycles"])
        self.assertEqual(report.counts["entry-targets"], 3)

    def test_ordinary_and_testonly_api_variants_keep_their_entry_roles(self):
        # An ordinary same-header variant is a supported entry for production
        # clients. A testonly same-header variant is an entry for tests, while
        # its production use remains Bazel's testonly check.
        report = run(api("canvas"), api("ink"),
                     target("//ink:debug_api", hdrs=("//ink:public.h",)),
                     target("//ink:test_api", hdrs=("//ink:public.h",), testonly=True),
                     target("//app:main", kind="cc_binary", deps=("//ink:debug_api",)),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//ink:test_api",)),
                     target("//app:diagnostics", kind="cc_binary", deps=("//ink:test_api",)))

        self.assertFalse(report.failed, report.render())
        self.assertIn("left to Bazel", report.render())
        self.assertEqual(report.counts["entry-targets"], 2)
        self.assertEqual([finding.rule for finding in report.ordered(not_checked=True)],
                         ["cycles", "cycles", "entry-targets"])

    def test_foreign_implementation_bypass_even_from_tests(self):
        report = run(api("canvas"), api("ink"), target("//ink:storage"),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//ink:storage",)),
                     target("//app:main", kind="cc_binary", implementation_deps=("//ink:storage",)))
        findings = report.ordered(not_checked=False)
        self.assertEqual([f.rule for f in findings], ["entry-targets", "entry-targets"])
        self.assertTrue(all("//ink:api" in f.reason for f in findings))

    def test_exact_explicit_package_grant_allows_implementation(self):
        report = run(api("canvas"), api("ink"),
                     target("//ink:storage", visibility=("//canvas:__pkg__", "//canvas/tests:__pkg__")),
                     target("//canvas:renderer", deps=("//ink:storage",)),
                     target("//canvas:decoder", implementation_deps=("//ink:storage",)),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//ink:storage",)))
        self.assertFalse(report.failed, report.render())
        self.assertFalse(report.ordered(not_checked=True), report.render())

    def test_public_visibility_fails_but_inherited_grants_are_uncertain(self):
        for visibility, explicit in ((("//visibility:public",), True),
                                     (("//visibility:public", "//canvas:__pkg__"), True),
                                     (("//canvas:__pkg__",), False)):
            with self.subTest(visibility=visibility, explicit=explicit):
                implementation = target("//ink:storage")
                implementation = Target(implementation.label, implementation.kind,
                                        implementation.location,
                                        (*implementation.attributes,
                                         attribute("visibility", visibility, explicit=explicit)))
                report = run(api("canvas"), api("ink"), implementation,
                             target("//canvas:renderer", deps=("//ink:storage",)))
                self.assertEqual(report.failed, explicit, report.render())
                if not explicit:
                    self.assertTrue(report.ordered(not_checked=True))

    def test_package_group_or_subtree_grants_are_not_checked(self):
        for visibility in (("//permissions:friends",), ("//canvas:__subpackages__",)):
            with self.subTest(visibility=visibility):
                report = run(api("canvas"), api("ink"),
                             target("//ink:storage", visibility=visibility),
                             target("//canvas:renderer", deps=("//ink:storage",)))
                self.assertFalse(report.failed)
                self.assertTrue(report.ordered(not_checked=True))

    def test_test_helpers_without_component_boundary(self):
        report = run(api("canvas"), target("//helpers:allocator", testonly=True),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//helpers:allocator",)))
        self.assertFalse(report.failed)
        self.assertEqual(report.counts["entry-targets"], 1)
        self.assertEqual([f.rule for f in report.ordered(not_checked=True)], ["cycles"])
        report = run(api("canvas"), target("//helpers:allocator", testonly=True),
                     target("//canvas:renderer", deps=("//helpers:allocator",)))
        self.assertFalse(report.failed)
        self.assertIn("left to Bazel", report.render())

    def test_owned_testonly_implementation_still_requires_selected_test_access(self):
        for visibility, permitted in ((None, False), (("//canvas/tests:__pkg__",), True)):
            with self.subTest(visibility=visibility):
                report = run(api("canvas"), api("ink"),
                             target("//ink:storage", testonly=True, visibility=visibility),
                             target("//canvas/tests:tests", kind="cc_test", deps=("//ink:storage",)))
                self.assertEqual(report.failed, not permitted, report.render())

    def test_parent_child_composition_and_same_owner_edges_pass(self):
        report = run(api("canvas", deps=("//canvas/internal/cache:api", "//canvas/impl:renderer")),
                     api("canvas/internal/cache"),
                     target("//canvas/impl:renderer", deps=("//canvas:api",)),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//canvas:api",)))
        self.assertFalse(report.failed, report.render())
        self.assertFalse(report.ordered(not_checked=True), report.render())
        self.assertEqual(report.counts["cycles"], 4)

    def test_nested_component_cycle_remains_a_cycle(self):
        report = run(api("canvas", deps=("//canvas/internal/cache:api",)),
                     api("canvas/internal/cache", deps=("//canvas:api",)))
        findings = report.ordered(not_checked=False)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule, "cycles")
        self.assertIn("//canvas:api -> //canvas/internal/cache:api -> //canvas:api", findings[0].reason)

    def test_cycles_use_inherited_test_and_implementation_ownership(self):
        report = run(api("canvas"), api("ink"),
                     target("//canvas/tests:tests", kind="cc_test", deps=("//ink:api",)),
                     target("//ink/impl:storage", deps=("//canvas:api",)))
        self.assertEqual([f.rule for f in report.ordered(not_checked=False)], ["cycles"])

    def test_implementation_grant_does_not_erase_component_cycle(self):
        report = run(api("canvas", deps=("//ink:storage",)),
                     api("ink", deps=("//canvas:api",)),
                     target("//ink:storage", visibility=("//canvas:__pkg__",)))
        self.assertEqual([f.rule for f in report.ordered(not_checked=False)], ["cycles"])

    def test_select_branches_do_not_assemble_a_definite_cycle(self):
        conditional = (Selector((Choice("//mode:fast", ("//ink:api",)),
                                 Choice("//conditions:default", ())), True, ""),)
        report = run(api("canvas", selectors=conditional), api("ink", deps=("//canvas:api",)))
        self.assertFalse(report.failed)
        self.assertIn("conditional component edge", report.render())

    def test_mutually_exclusive_select_edges_do_not_form_a_cycle(self):
        canvas_deps = (Selector((Choice("//mode:debug", ("//ink:api",)),
                                 Choice("//conditions:default", ())), True, ""),)
        ink_deps = (Selector((Choice("//mode:debug", ()),
                              Choice("//conditions:default", ("//canvas:api",))), True, ""),)

        report = run(api("canvas", selectors=canvas_deps),
                     api("ink", selectors=ink_deps))

        self.assertFalse(report.failed, report.render())
        cycle_limitations = [finding for finding in report.ordered(not_checked=True)
                             if finding.rule == "cycles"]
        self.assertEqual(len(cycle_limitations), 2, report.render())
        self.assertTrue(all("conditional component edge" in finding.reason
                            for finding in cycle_limitations))

    def test_conditional_bypass_is_nonblocking_alongside_definite_violation(self):
        conditional = (Selector((Choice("//mode:fast", ("//ink:storage",)),
                                 Choice("//conditions:default", ())), True, ""),)
        report = run(api("canvas", selectors=conditional), api("ink"), target("//ink:storage"),
                     target("//canvas:renderer", deps=("//ink:storage",)))
        self.assertTrue(report.failed)
        self.assertIn("conditional dependency may bypass", report.render())

    def test_default_only_selector_is_unconditional(self):
        constant = (Selector((Choice("//conditions:default", ("//ink:api",)),), True, ""),)
        report = run(api("canvas", selectors=constant), api("ink", deps=("//canvas:api",)))
        self.assertTrue(report.failed)
        self.assertFalse(report.ordered(not_checked=True))

    def test_missing_alias_or_unknown_kind_is_not_checked_external_is_excluded(self):
        report = run(api("canvas", deps=("//ink:missing", "//ink:alias", "@external//:api")),
                     target("//ink:alias", kind="alias"))
        self.assertFalse(report.failed)
        self.assertEqual(len(report.ordered(not_checked=True)), 4)
        self.assertNotIn("@external", report.render())

    def test_unsupported_boundary_does_not_create_false_bypass(self):
        report = run(target("//canvas:api", kind="custom_cc"),
                     target("//canvas:renderer", deps=("//ink:storage",)),
                     api("ink"), target("//ink:storage"))
        self.assertFalse(report.failed)
        self.assertIn("uncertain component boundary", report.render())

    def test_unknown_attribute_is_visible_without_hiding_supported_violation(self):
        client = target("//canvas:renderer", deps=("//ink:storage",))
        client = Target(client.label, client.kind, client.location,
                        (*client.attributes[:-1], Attribute("implementation_deps", "UNKNOWN", True,
                                                          limitation="unsupported attribute type")))
        report = run(api("canvas"), api("ink"), target("//ink:storage"), client)
        self.assertTrue(report.failed)
        self.assertEqual([f.rule for f in report.ordered(not_checked=True)], ["cycles", "entry-targets"])

    def test_findings_repeat_and_sort_independently_of_target_order(self):
        targets = (api("canvas", deps=("//ink:api",)),
                   api("ink", deps=("//canvas:api",)),
                   target("//canvas:renderer", deps=("//ink:storage",)),
                   target("//ink:storage"))
        self.assertEqual(run(*targets).render(), run(*reversed(targets)).render())


if __name__ == "__main__":
    unittest.main()
