"""Focused include-rule examples; no compiler or Bazel toolchain is required."""

from pathlib import Path
import tempfile
import unittest

from discovery import Inventory
from facts import (Attribute, BuildFacts, Choice, File, Location, OperationalError,
                   Selector, Target)
from includes import Directive, check_includes, directives
from ownership import Ownership
from report import Report


def attribute(name, value):
    return Attribute(name, "STRING_LIST", True, value)


def conditional(name, value):
    return Attribute(name, "STRING_LIST", True, selectors=(Selector((
        Choice("//:choice", (value,)), Choice("//conditions:default", ())), True, ""),))


def target(package, name="api", headers=(), sources=(), deps=(), extra=(), kind="cc_library"):
    def labels(paths):
        return tuple(p if p.startswith("//") else f"//{package}:{p}" for p in paths)
    return Target(f"//{package}:{name}", kind, Location(package + "/BUILD.bazel"), (
        attribute("hdrs", labels(headers)), attribute("srcs", labels(sources)),
        attribute("deps", tuple(deps)), *extra))


class IncludesTests(unittest.TestCase):
    def run_check(self, files, targets, generated=()):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for path, text in files.items():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_text(text)
            packages = tuple(sorted({t.label[2:].split(":")[0] for t in targets}))
            inventory = Inventory(tuple(sorted(files)), (), packages)
            facts = BuildFacts(tuple(targets), tuple(generated))
            report = Report()
            check_includes(root, inventory, facts, Ownership(facts, inventory), report)
            return report

    def test_scanner_ignores_comments_strings_and_raw_strings(self):
        source = '''// #include "bad/a.h"
/*
#include "bad/b.h"
*/
const char* text = R"tag(
#include "bad/c.h"
)tag";
const char* quoted = "#include \\\"bad/d.h\\\"";
#include /*comment*/ "good.h" // trailing
'''
        self.assertEqual(directives(source), (Directive(
            9, "good.h", False, False),))

    def test_guards_and_spliced_directives_preserve_lines(self):
        parsed = directives('#ifndef BIRD_H\n#define BIRD_H\n#inc\\\nlude "one.h"\n'
                            '#if FLAG\n#include "two.h"\n#endif\n#endif\n')
        self.assertEqual([(d.line, d.spelling, d.conditional) for d in parsed],
                         [(3, "one.h", False), (6, "two.h", True)])

    def test_api_chain_reaches_impl_with_complete_path(self):
        report = self.run_check({
            "bird/bird.h": '#include "middle.h"',
            "bird/middle.h": '#include "impl/state.h"',
            "bird/impl/state.h": "",
        }, [target("bird", headers=("bird.h",))])
        self.assertTrue(report.failed)
        self.assertIn("bird/bird.h -> bird/middle.h -> bird/impl/state.h",
                      report.ordered(not_checked=False)[0].reason)

    def test_preprocessor_or_membership_condition_makes_potential(self):
        for membership in (False, True):
            t = target("bird", headers=("bird.h",))
            if membership:
                t = Target(t.label, t.kind, t.location,
                           (conditional("hdrs", "//bird:bird.h"), *t.attributes[1:]))
            report = self.run_check({
                "bird/bird.h": ('#if FEATURE\n' if not membership else '')
                               + '#include "impl/state.h"\n'
                               + ('#endif\n' if not membership else ''),
                "bird/impl/state.h": "",
            }, [t])
            self.assertFalse(report.failed)
            self.assertTrue(any("closure" in f.reason for f in report.findings))

    def test_strip_prefix_mapping_and_platform_headers(self):
        report = self.run_check({
            "bird/include/bird/bird.h": '#include <signal.h>\n#include <termios.h>\n'
                                       '#include <vector>\n#include <gtest/gtest.h>\n',
            "client/client.h": '#include "bird/bird.h"',
        }, [target("bird", headers=("include/bird/bird.h",), extra=(
                attribute("strip_include_prefix", "include"),)),
            target("client", headers=("client.h",))])
        self.assertFalse(report.findings)
        self.assertEqual(report.counts["includes"], 1)

    def test_prefix_and_include_directory_mapping(self):
        report = self.run_check({
            "bird/public/bird.h": '#include "state.h"',
            "bird/helpers/state.h": "",
            "client/client.h": '#include "flock/bird.h"',
        }, [target("bird", headers=("public/bird.h",), extra=(
                attribute("strip_include_prefix", "public"),
                attribute("include_prefix", "flock"),
                attribute("includes", ("helpers",)))),
            target("client", headers=("client.h",))])
        self.assertFalse(report.findings)
        self.assertEqual(report.counts["includes"], 2)

    def test_unrelated_private_access_fails_but_parent_composition_passes(self):
        files = {"bird/bird.h": '#include "bird/internal/nest/nest.h"',
                 "bird/internal/nest/nest.h": "",
                 "fish/fish.h": '#include "bird/internal/nest/nest.h"'}
        targets = [target("bird", headers=("bird.h",)),
                   target("bird/internal/nest", headers=("nest.h",)),
                   target("fish", headers=("fish.h",))]
        report = self.run_check(files, targets)
        self.assertEqual(len(report.ordered(not_checked=False)), 1)
        self.assertEqual(report.ordered(not_checked=False)[0].location.path, "fish/fish.h")

    def test_selected_tests_need_both_grant_and_dependency(self):
        for grant, dependency, allowed in ((True, True, True), (False, True, False),
                                           (True, False, False)):
            provider = target("bird/internal/nest", headers=("nest.h",), extra=(
                attribute("visibility", ("//fish/tests:__pkg__",) if grant else
                          ("//visibility:public",)),))
            consumer = target("fish/tests", "tests", sources=("tests.cpp",),
                              deps=(provider.label,) if dependency else (), kind="cc_test")
            report = self.run_check({"fish/fish.h": "", "bird/bird.h": "",
                                     "bird/internal/nest/nest.h": "",
                                     "fish/tests/tests.cpp": '#include "bird/internal/nest/nest.h"'},
                                    [target("fish", headers=("fish.h",)),
                                     target("bird", headers=("bird.h",)), provider, consumer])
            self.assertEqual(report.failed, not allowed)

    def test_grant_does_not_excuse_api_closure_to_impl(self):
        report = self.run_check({"bird/bird.h": '#include "fish/impl/state.h"',
                                 "fish/fish.h": "", "fish/impl/state.h": ""}, [
            target("bird", headers=("bird.h",)), target("fish", headers=("fish.h",)),
            target("fish", "state", headers=("impl/state.h",), extra=(
                attribute("visibility", ("//bird:__pkg__",)),))])
        self.assertTrue(report.failed)
        self.assertEqual(report.ordered(not_checked=False)[0].rule, "includes")

    def test_missing_computed_ambiguous_and_generated_are_limitations(self):
        report = self.run_check({"bird/bird.h": '#include HEADER\n#include "missing.h"\n'
                                  '#include "shared.h"\n#include "generated.h"',
                                 "fish/include/shared.h": "", "cat/include/shared.h": ""}, [
            target("bird", headers=("bird.h", "generated.h"), extra=(
                attribute("strip_include_prefix", "."),)),
            target("fish", headers=("include/shared.h",), extra=(
                attribute("strip_include_prefix", "include"),)),
            target("cat", headers=("include/shared.h",), extra=(
                attribute("strip_include_prefix", "include"),))],
            [File("//bird:generated.h", Location("bird/BUILD.bazel"), "//bird:generate")])
        self.assertFalse(report.failed)
        reasons = "\n".join(f.reason for f in report.findings)
        for expected in ("computed", "unresolved", "ambiguous", "generated"):
            self.assertIn(expected, reasons)

    def test_early_closed_guard_does_not_exempt_separate_branch(self):
        parsed = directives('#ifndef BIRD_H\n#define BIRD_H\n#endif\n'
                            '#if FLAG\n#include "state.h"\n#endif\n')
        self.assertTrue(parsed[0].conditional)

    def test_application_without_component_is_a_clear_private_client(self):
        report = self.run_check({"bird/internal/nest/nest.h": "",
                                 "app/main.cpp": '#include "bird/internal/nest/nest.h"'}, [
            target("bird/internal/nest", headers=("nest.h",)),
            target("app", "main", sources=("main.cpp",), kind="cc_binary")])
        self.assertTrue(report.failed)
        self.assertEqual(report.ordered(not_checked=False)[0].rule, "private-access")

    def test_quoted_external_header_uses_dependency_evidence(self):
        report = self.run_check({"bird/bird.h": '#include "gtest/gtest.h"'}, [
            target("bird", headers=("bird.h",), deps=("@googletest//:gtest",))])
        self.assertFalse(report.findings)

    def test_unsupported_mapping_cannot_invent_definite_path(self):
        report = self.run_check({"bird/impl/state.h": "",
                                 "client/client.h": '#include "public/bird/impl/state.h"'}, [
            target("bird", headers=("impl/state.h",), extra=(
                Attribute("strip_include_prefix", "UNKNOWN", True, limitation="unknown type"),
                attribute("include_prefix", "public"))),
            target("client", headers=("client.h",))])
        # Exporting an impl header as its API is checked separately; the unknown
        # mapping must never manufacture the include edge from client/client.h.
        self.assertFalse(any(not f.not_checked and f.location.path == "client/client.h"
                             for f in report.findings))
        self.assertTrue(any("unresolved" in f.reason for f in report.findings))

    def test_conditional_mapping_produces_potential_closure(self):
        report = self.run_check({"bird/bird.h": '#include "private/impl/state.h"',
                                 "storage/impl/state.h": ""}, [
            target("bird", headers=("bird.h",)),
            target("storage", "state", headers=("impl/state.h",), extra=(
                attribute("strip_include_prefix", "."),
                conditional("include_prefix", "private")))])
        self.assertFalse(report.failed)
        self.assertTrue(any("closure" in f.reason for f in report.findings))

    def test_sibling_private_production_grant_requires_dependency(self):
        for grant in (True, False):
            provider = target("bird/internal/limits", headers=("limits.h",), extra=(
                attribute("visibility", ("//bird/internal/nest:__pkg__",) if grant else
                          ("//visibility:private",)),))
            consumer = target("bird/internal/nest", headers=("nest.h",), sources=("nest.cpp",),
                              deps=(provider.label,))
            report = self.run_check({"bird/internal/limits/limits.h": "",
                                     "bird/internal/nest/nest.h": "",
                                     "bird/internal/nest/nest.cpp":
                                     '#include "bird/internal/limits/limits.h"'},
                                    [provider, consumer])
            self.assertEqual(report.failed, not grant)

    def test_unsupported_directive_forms_are_visible(self):
        parsed = directives('#include_next "bird.h"\n#import "bird.h"\n%:include "bird.h"')
        self.assertEqual([d.spelling for d in parsed], [None, None, None])

    def test_conditional_source_membership_makes_private_access_potential(self):
        client = target("fish", sources=("fish.cpp",))
        client = Target(client.label, client.kind, client.location,
                        (conditional("srcs", "//fish:fish.cpp"), *client.attributes[:1],
                         *client.attributes[2:]))
        report = self.run_check({"fish/fish.cpp": '#include "bird/internal/nest/nest.h"',
                                 "bird/internal/nest/nest.h": ""}, [
            client, target("bird/internal/nest", headers=("nest.h",))])
        self.assertFalse(report.failed)
        self.assertTrue(any(f.rule == "private-access" for f in report.findings))

    def test_uncertain_second_source_context_does_not_hide_definite_access(self):
        regular = target("fish", sources=("fish.cpp",))
        unknown = target("fish", "maybe_tests", sources=("fish.cpp",), extra=(
            Attribute("testonly", "UNKNOWN", True, limitation="unknown test context"),))
        unknown = Target(unknown.label, unknown.kind, unknown.location,
                         (conditional("srcs", "//fish:fish.cpp"),
                          *tuple(a for a in unknown.attributes if a.name != "srcs")))
        report = self.run_check({"fish/fish.cpp": '#include "bird/internal/nest/nest.h"',
                                 "bird/internal/nest/nest.h": ""}, [
            regular, unknown, target("bird/internal/nest", headers=("nest.h",))])
        self.assertTrue(report.failed)
        self.assertEqual(report.ordered(not_checked=False)[0].rule, "private-access")

    def test_required_source_read_failure_is_operational(self):
        with tempfile.TemporaryDirectory() as directory:
            inventory = Inventory(("bird/missing.h",), (), ("bird",))
            facts = BuildFacts((), ())
            with self.assertRaisesRegex(OperationalError, "Cannot read required source"):
                check_includes(Path(directory), inventory, facts,
                               Ownership(facts, inventory), Report())

    def test_indirect_and_unsupported_source_membership_are_visible(self):
        indirect = target("bird", sources=("source_group",))
        unknown = target("bird", "unknown", extra=(
            Attribute("srcs", "UNKNOWN", True, limitation="unsupported source type"),))
        unknown = Target(unknown.label, unknown.kind, unknown.location,
                         tuple(a for a in unknown.attributes
                               if a.name != "srcs" or a.limitation))
        report = self.run_check({}, [indirect, unknown])
        self.assertFalse(report.failed)
        self.assertIn("source membership is indirect", report.render())
        self.assertIn("unsupported source type", report.render())

    def test_repository_root_strip_prefix_mapping(self):
        provider = target("", headers=("public.h",), extra=(
            attribute("strip_include_prefix", "/"), attribute("include_prefix", "top")))
        provider = Target(provider.label, provider.kind, Location("BUILD.bazel"),
                          provider.attributes)
        report = self.run_check({"public.h": "",
                                 "client/client.h": '#include "top/public.h"'}, [
            provider, target("client", headers=("client.h",))])
        self.assertFalse(report.findings, report.render())
        self.assertEqual(report.counts["includes"], 1)

    def test_real_empty_strip_default_does_not_invent_package_relative_alias(self):
        for explicit, strip, resolves in ((False, "", False), (True, "", False),
                                         (True, ".", True)):
            report = self.run_check({
                "bird/internal/nest/state.h": "",
                "fish/fish.h": '#include "state.h"',
            }, [target("bird/internal/nest", headers=("state.h",), extra=(
                    Attribute("strip_include_prefix", "STRING", explicit, strip),
                    Attribute("include_prefix", "STRING", False, ""))),
                target("fish", headers=("fish.h",))])
            self.assertEqual(report.failed, resolves)
            if not resolves:
                self.assertTrue(any("unresolved" in f.reason for f in report.findings))

    def test_include_prefix_alone_strips_package_with_empty_strip_default(self):
        report = self.run_check({"bird/state.h": "",
                                 "fish/fish.h": '#include "flock/state.h"'}, [
            target("bird", headers=("state.h",), extra=(
                Attribute("strip_include_prefix", "STRING", False, ""),
                attribute("include_prefix", "flock"))),
            target("fish", headers=("fish.h",))])
        self.assertFalse(report.findings)
        self.assertEqual(report.counts["includes"], 1)

    def test_guard_alternate_elifdef_and_elifndef_are_conditional(self):
        for alternate in ("elifdef", "elifndef"):
            parsed = directives('#ifndef BIRD_H\n#define BIRD_H\n'
                                f'#{alternate} FLAG\n#include "state.h"\n#endif\n')
            self.assertTrue(parsed[0].conditional)

    def test_invalid_include_prefixes_are_visible_without_inventing_edges(self):
        for prefix in ("/public", "../public", "flock/../public"):
            report = self.run_check({"bird/state.h": "",
                                     "fish/fish.h": '#include "' + prefix + '/state.h"'}, [
                target("bird", headers=("state.h",), extra=(
                    Attribute("strip_include_prefix", "STRING", False, ""),
                    attribute("include_prefix", prefix))),
                target("fish", headers=("fish.h",))])
            self.assertFalse(report.failed)
            self.assertTrue(any("unsupported include_prefix" in f.reason for f in report.findings))
            self.assertEqual(report.counts["includes"], 0)

    def test_limitation_does_not_hide_definite_violation(self):
        report = self.run_check({"bird/bird.h": '#include HEADER\n#include "impl/state.h"',
                                 "bird/impl/state.h": ""}, [target("bird", headers=("bird.h",))])
        self.assertTrue(report.failed)
        self.assertTrue(report.ordered(not_checked=True))


if __name__ == "__main__":
    unittest.main()
