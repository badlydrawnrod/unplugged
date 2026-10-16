# A small, deterministic architecture checker

Status: steps 1–4 complete.
Created: 2026-10-16.
Last progress update: 2026-10-16.

## Progress

Keep this checklist and the latest handoff current as each implementation step
is completed. The [tooling README](../tools/architecture/README.md) owns the
supported behavior and limitations; this plan tracks delivery and verification.

- [x] **Step 1: Create the minimal foundation.** Git discovery, structured Bazel
  fact loading, private fact records, documentation, and focused fixtures.
- [x] **Step 2: Implement the small checker.** Ownership helper, findings,
  `check.py`, include rules, entry-target checks, and component cycles.
- [x] **Step 3: Prove behavior with focused fixtures.** Passing, failing, and
  limitation cases for all four rules and their interactions; completion-criteria
  coverage reviewed and missing fixtures added.
- [x] **Step 4: Integrate and finish.** Verification entry-point integration,
  failure-propagation tests, final documentation, and completion checks.

### Step 4 handoff — 2026-10-16

Continued on `spike/simple-enforcement` from committed step 3 (`445eccd`).
Delegated integration and documentation to GPT-6 Luna with high reasoning, then
reviewed the combined behavior and ran the completion checks. No commits were
made during step 4.

`tools/verify.py` now runs the architecture checker and complete focused
architecture fixture suite after the existing format and acceptance checks.
It preserves script-relative repository resolution, `--clang-format` forwarding,
continued checks after child failures, and an aggregate nonzero status when any
check fails. Bazel build/default/debug tests remain separate commands.

Added `tools/architecture/verify_tests.py` with four public-command fixtures.
They copy the actual verification entry point into a temporary checkout, use
small child commands and a real unittest discovery suite, and verify checker
violation/operational statuses, architecture fixture failures, prior failures
with later checks still running, invocation from another directory, and
`--clang-format` argument forwarding. These tests run in the architecture suite
without recursively invoking the repository's full verification.

Updated the tooling README, repository guide, and acceptance guide to describe
the integrated commands, supported failure behavior, Git/pinned-Bazel
requirements, and unchanged coverage limitations. The existing CI invocation
already runs the new checks; only its display name changed.

Verification completed successfully:

- `python3 -m unittest discover -s tools/architecture -p '*_tests.py'`: all 95
  architecture tests passed, including the four verification integration tests.
- `python3 tools/architecture/check.py`: exit 0, no violations, 19 explicit
  not-checked findings. Checked counts remain 112 include directives, 17
  private-header directives, 85 entry-target edges, and 69 cycle edges.
- `python3 tools/verify.py`: passed formatting for 63 C++ files, traceability
  for 44 scenarios/48 bindings, 20 acceptance fixtures, the repository
  architecture audit, and all 95 architecture fixtures.
- `bazel build //:editor`, `bazel test //...`, and
  `bazel test -c dbg //...` passed. Both test modes reported 20 passing targets
  using cached results. Whitespace checks passed.

The delegated verification attempt encountered the sandbox's Bazel cache access
restriction. The final complete run used approved cache access and passed.
There are no outstanding verification failures. Product behavior and existing
compiler-based public-header assertions are unchanged.

All four steps are complete. Coverage remains intentionally bounded: global
syntactic include aliases, unresolved preprocessor/selector combinations,
indirect membership, unavailable generated headers, target aliases, unsupported
visibility forms, and unowned utility/application cycle edges remain visible
limitations. The current 19 findings comprise three helper/policy API-ownership
limitations and 16 cycle edges involving unowned application/helper targets.

### Step 3 handoff — 2026-10-16

Continued on `spike/simple-enforcement` from committed step 2 (`b8fd4f8`).
Delegated the bounded fixture work to GPT-6 Luna with high reasoning, then
reviewed the combined changes. No commits were made during step 3.

Added nine tests across `ownership_tests.py`, `includes_tests.py`,
`dependencies_tests.py`, and `check_integration_tests.py`, and extended existing
grant fixtures. The additions cover supported nested boundaries beneath
unsupported ancestors, uncertain API variants, misleading `test_api` names,
`detail/` access, conditional private grants, ordinary/testonly API variants,
mixed public/package visibility, grants on `implementation_deps`, and mutually
exclusive selector edges. No production checker changes were needed.

The new real Git/Bazel lifecycle fixture starts with passing parent/nested
composition and nonzero checked counts for all four rules. It adds an untracked
conventional component and verifies its private-header violation alongside a
computed-include limitation. Repairing the private include leaves a nonblocking
limitation; repeated discovery, fact loading, and auditing produce identical
inventories, facts, and rendered findings. No registry or checker edits occur
between these fixture states.

Completion-criteria coverage review (existing and added fixtures):

| Requirement | Fixture evidence |
| --- | --- |
| Passing, failing, and limitation cases for every rule | Include/dependency suites plus passing and failing real-pipeline fixtures; command suite verifies exit statuses 0/1/2. |
| Comments, strings, guards, and include chains | Include scanner fixtures mask comments/ordinary/raw strings, preserve physical lines, and report full API-to-`impl/` paths. |
| Parent/private composition and selected test access | Include fixtures allow parent composition and require both exact grants and direct dependencies for selected tests; dependency fixtures preserve selected test implementation access. |
| Ordinary/test APIs and legitimate implementation grants | Ownership/dependency fixtures distinguish matching APIs, unsupported/conditional variants, misleading names, testonly context, explicit grants, inherited grants, and public visibility. Grants cannot excuse API include closure or remove cycle edges. |
| Inherited and nested ownership | Ownership and dependency fixtures cover inherited test/implementation owners, distinct nested components, and supported nested boundaries overriding unsupported ancestors. |
| Parent-to-child composition, parent/child cycles, and same-owner edges | Dependency fixtures pass composition and same-owner round trips, retain the exact parent/child cycle witness, and map inherited tests/implementation targets into cycles. |
| Conditional paths | Include/header/source/mapping/grant fixtures remain potential findings; dependency selectors, complementary mutually exclusive cycle edges, and default-only constant selectors retain their classifications. |
| Visible limitations alongside definite violations | Include/dependency fixtures and the all-four-rule real pipeline retain violations alongside unsupported inputs; the lifecycle repair proves limitations alone remain nonblocking. |
| Operational failures | Discovery/query fixtures cover real Git/Bazel errors and malformed output; include fixtures cover unreadable required source; command fixtures return status 2 without a successful report. |
| Determinism and automatic component discovery | Reordered facts/targets produce identical reports; repeated real loads agree; a newly added untracked API package is discovered and checked without registry edits. |
| Existing repository arrangements and compiler assertions | Current checker succeeds with nonzero checked counts, POSIX headers remain supported, and default/debug Bazel tests preserve public-header assertions and existing product behavior. |

Verification completed successfully:

- `python3 -m unittest discover -s tools/architecture -p '*_tests.py'`: 91 tests
  passed (29 include, 20 dependency, 14 ownership/report, 6 command, 2 checker
  integration, and the original 20 foundation tests).
- `python3 tools/architecture/check.py`: exit 0, no violations, the same 19
  explicit not-checked findings. Checked counts remain 112 include directives,
  17 private-header directives, 85 entry-target edges, and 69 cycle edges.
- `python3 tools/verify.py`, `bazel build //:editor`, `bazel test //...`, and
  `bazel test -c dbg //...` passed. Each Bazel test mode reported 20 passing
  test targets using cached results. Whitespace checks passed.

Supported forms and limitations are unchanged. `tools/verify.py` remains
unchanged; running the checker and its fixtures through that entry point,
verification failure-propagation tests, and final integration documentation
belong to step 4. There are no outstanding verification failures.

Next: implement step 4 from this tested fixture baseline. Preserve the checker,
the completed coverage audit, and all unrelated working-tree edits.

### Step 2 handoff — 2026-10-16

Implemented on `spike/simple-enforcement`, continuing from the committed step 1
foundation (`a66d09b`). No commits were made during step 2.

Added under `tools/architecture/`:

- `ownership.py` and `ownership_tests.py`: shared nearest-component ownership,
  inherited test/implementation ownership, distinct nested components,
  conservative API-variant recognition, selector references, and narrow explicit
  package-grant evidence.
- `report.py`: deterministic findings, separate violations and coverage
  limitations, and per-rule checked/not-checked counts.
- `includes.py` and `includes_tests.py`: comment/string-aware directive scanning,
  literal include resolution, API closure into `impl/`, direct private-header
  checks, conditional-path handling, and visible unsupported inputs.
- `dependencies.py` and `dependencies_tests.py`: direct entry-target checks and
  unconditional component cycles, preserving nested components and implementation
  grants while excluding conditional edges from definite cycles.
- `check.py`, `check_tests.py`, and `check_integration_tests.py`: public command
  with script-relative default root and `--repo`, exit statuses 0/1/2 for
  passing/violations/operational errors, command fixtures, and a real Git/Bazel
  pipeline fixture exercising all four rules alongside a coverage limitation.

Updated the tooling README with implemented behavior, exact supported grant and
mapping forms, limitations, output semantics, and commands. Narrow private-header
access requires an explicit exact-package provider grant plus a declared direct
dependency; parent production composition and same-owner use remain supported.
Public visibility does not grant an API bypass; inherited narrow grants remain
not checked. Grants never excuse
API include closure into `impl/` or erase cycle edges. Default and prefix-only
mapping behavior was checked against the pinned `rules_cc` implementation.

Verification completed successfully:

- `python3 -m unittest discover -s tools/architecture -p '*_tests.py'`: 82 tests
  passed, including 27 include, 18 dependency, 10 ownership/report, 6 command,
  1 checker integration, and the original 20 foundation tests. Integration
  fixtures use actual Git and pinned Bazel loading-phase queries without a
  compiler or SDK. Reordered facts produce identical output.
- `python3 tools/architecture/check.py`: exit 0, no violations, 19 explicit
  not-checked findings. Checked counts: 112 resolved include directives,
  17 private-header directives, 85 entry-target edges, and 69 cycle edges.
- The 19 limitations are three helper/policy libraries without supported API
  ownership and 16 cycle edges involving unowned application/helper targets.
  They remain visible; supported-rule success does not rely on skipping all
  checks.
- `python3 tools/verify.py`, `bazel build //:editor`, `bazel test //...`, and
  `bazel test -c dbg //...` passed. Both Bazel test modes reported 20 passing
  test targets using cached results.

Remaining limits include global syntactic alias lookup rather than configured
compiler search paths, unresolved preprocessor/selector combinations, indirect
membership, unavailable generated headers, target aliases, and unsupported
visibility forms. Product behavior and compiler-based public-header assertions
are unchanged. `tools/verify.py` remains unchanged; integration belongs to step 4.
There are no outstanding verification failures.

Next: review the existing fixtures against every step 3 completion requirement
and add missing cases before proceeding to verification integration. Useful
additional ownership fixtures include supported nested boundaries overriding
unsupported ancestors and uncertain API variants. Do not treat the current core
fixtures as completion of the remaining plan.

### Step 1 handoff — 2026-10-16

Implemented on `spike/simple-enforcement`, starting at the same commit as
`develop` (`7eedb1f23527f2595fe9e4047bfb21ba42467b72`) with the staged plan carried
over. No commits were made during implementation.

Added under `tools/architecture/`:

- `README.md`: operating contract, four planned rules, supported foundation
  forms, limitations, and test commands.
- `discovery.py` and `discovery_tests.py`: deterministic NUL-delimited Git
  inventory of active C/C++ inputs and BUILD packages, excluding ignored
  untracked output, deleted files, and symlink paths or ancestors.
- `facts.py`: small immutable records for locations, targets, attributes,
  selector segments and choices, file facts, and coverage limitations, plus a
  distinct `OperationalError`.
- `query.py`, `query_tests.py`, and `query_integration_tests.py`: pinned Bazel
  version verification and explicit package queries using structured
  loading-phase output with selectors retained. Code dependencies remain
  separate from data and tools; header/source membership, visibility, test
  context, and simple include-mapping attributes are retained.

Verification completed successfully:

- `python3 -m unittest discover -s tools/architecture -p '*_tests.py'`: all 20
  foundation tests passed (8 discovery, 11 query, 1 real-Bazel integration).
  Fixtures cover actual Git discovery, selector concatenation and branch
  retention, empty/default values, visibility and test context, generated
  inputs, manual/incompatible targets, package names containing spaces,
  repeatability, coverage limitations, and operational failures.
- A real repository discovery and fact load with Bazel 9.2.0 found 63 C/C++
  inputs, 27 active BUILD packages, 45 targets, and 132 file facts, with no
  unsupported inspected attributes.
- `python3 tools/verify.py` passed: formatting, acceptance traceability, and all
  20 traceability fixture tests.
- `bazel build //:editor`, `bazel test //...`, and
  `bazel test -c dbg //...` passed. Both test modes reported 20 passing test
  targets using cached results.
- Whitespace checks passed for all added files. Review found no material issues.

No rule evaluation or architecture verdict exists yet. Ownership inference,
finding/report definitions, the public command, and rule fixtures remain to be
implemented. Membership labels and include mappings are retained without
resolving indirect inputs, aliases, or headers. Unsupported inspected attribute
types and unknown query record kinds retain coverage limitations; tool failures
and malformed tool output raise operational errors. `tools/verify.py` has not
been changed to run the new tooling. Product behavior and existing public-header
assertions are unchanged. There are no outstanding verification failures.

## Direction for the next session

Build a useful checker for a few observable architectural mistakes. Complete
coverage is not a requirement. Unsupported arrangements should be visible in the
report without blocking development.

All implementation steps are complete. For future extensions, inspect Git
status before making changes and preserve the current implementation. The
original baseline was `develop` without `tools/architecture/`; the implementation
recorded above is now the starting point. No older architecture plan or abandoned
checker implementation is needed.

Read [AGENTS.md](../AGENTS.md) and its linked guidance before implementation. The
[repository guide](../guidance/repository.md) describes the existing layout and
verification commands. Existing examples of Git-backed file discovery are in
`tools/acceptance/check_traceability.py` and `tools/check_format.py`; they may help
with the small discovery helper below. This work adds tooling and should preserve
existing product behavior.

## Session handoff and delegation

The implementation used this delegation approach; future bounded extensions can
follow the same pattern:

> Extend the architecture checker only for a concrete violation worth detecting.
> Preserve the completed implementation and fixture coverage. Use
> GPT-6 Luna subagents with high reasoning for bounded implementation tasks.
> Keep one lead agent responsible for shared interfaces, policy decisions,
> integration, review, and all completion checks. Add a focused fixture for each
> extension, complete verification, and report the results.

The lead should read this whole plan, the tooling README, and the contributor
guidance, inspect Git status and the current branch, and confirm the recorded
progress before delegating. Preserve completed work and unrelated changes. If a
separate checkout is needed, carry forward this plan and the completed checker
rather than restarting from `develop`. Do not reuse an abandoned checker
implementation. Routine implementation choices can be resolved under this plan
without another architecture-planning round.

### Sequence and responsibilities

1. **Agree on small shared interfaces first.** The lead defines the private
   representation of the few build facts and findings the rules consume. Retain
   repository-relative locations, target labels and kinds, source/header
   membership, code dependencies with conditional choices, test context, explicit
   grants, and simple include mappings. Findings carry a rule, location, reason,
   and definite-violation or not-checked classification; operational failures
   remain distinct. Conditional potential findings use the not-checked category.
   Establish one shared ownership helper using the component ownership rules
   below. Keep these interfaces small and local to this checker.
2. **Delegate the foundation.** One worker implements Git discovery and
   structured Bazel fact loading, with its focused tests. The lead reviews a real
   query result from the pinned Bazel version, retained selectors, and failure
   propagation before downstream implementation relies on those facts. Rule
   workers may explore examples meanwhile; wait for the reviewed interfaces
   before implementing against them.
3. **Delegate the two rule groups.** Once the foundation is reviewed, workers
   may implement the include rules and dependency rules in parallel using the
   shared facts and ownership helper. Each worker supplies passing, failing, and
   limitation fixtures for its rules.
4. **Integrate and review.** The lead connects the command and verification
   entry point, reviews the combined behavior, updates the tooling contract and
   commands, and runs every completion check below. Worker reports and isolated
   tests supplement this final verification.

Suggested file ownership; the lead chooses exact filenames before dispatch:

| Assignment | Owned work |
| --- | --- |
| Foundation worker | Discovery/query helpers and their fixture tests |
| Include worker | Directive scanning, include resolution, API include closure, private access, and their fixture tests |
| Dependency worker | Entry-target checks, unconditional component cycles, and their fixture tests |
| Lead | Shared fact/finding definitions, ownership helper, `check.py`, README, `tools/verify.py`, and integration tests |

Give each worker this plan, applicable guidance, its exact owned files, the agreed
interfaces, and the relevant fixture requirements. Workers should request shared
interface changes through the lead and preserve concurrent edits. Each handoff
must list changed files, supported forms, limitations, test commands and results,
and any unresolved decisions. Wait for and review each prerequisite before
starting dependent work.

The lead must review conditional include/dependency paths, explicit implementation
grants and selected test access, and operational-error versus coverage-limit
handling. Document the exact supported grant forms, backed by both allowed-access
and API-bypass fixtures. Confirm discovery and query behavior with actual Git and
Bazel invocations as well as focused fixtures. Report remaining limitations
plainly; current repository success must not come from silently skipping all
checks. The final handoff includes the change summary, verification results, and
any outstanding failures or limitations.

## Operating contract

- Same files, evaluated build facts, tool versions, and options produce the same
  findings and exit status. Sort diagnostics by repository-relative path, line,
  and rule; keep machine-dependent timing out of findings.
- A definite violation of a supported rule fails the check. An unsupported or
  ambiguous construct produces a nonblocking **not checked** finding identifying
  its location, affected rule, and reason. Continue checks that are unaffected.
- Git/Bazel invocation failures and unreadable required inputs are operational
  errors and fail the command. Do not turn a failed audit into a successful empty
  report. Parser limitations or unrecognized source arrangements are coverage
  limitations, not operational errors.
- Report definite violations and coverage limitations separately, with a short
  checked/not-checked summary. Exit 0 means no supported-rule violation was found,
  not that the architecture was proved correct. Start with text output only.
- Derive facts from source, layout, and evaluated Bazel declarations. No component
  registry, role manifest, exception inventory, or checked-in generated graph.
- Source inspection needs no C++ compiler or platform SDK. Leave effective
  visibility and `testonly` enforcement to ordinary Bazel builds.

## Initial rules

| Rule | Supported check |
| --- | --- |
| Includes | Follow resolvable literal first-party includes and reject a supported API header whose include closure reaches `impl/`. Report the offending path. |
| Private access | Flag clear direct client includes of another component's `internal/` or `detail/` headers. Preserve parent/private composition and explicitly selected test access. |
| Entry targets | Flag clear direct production API bypasses in evaluated code dependencies. Respect narrowly granted implementation dependencies and test helpers; uncertain edge classification is not checked. |
| Cycles | Report component cycles made entirely from resolved, unconditional code-dependency edges. Ignore edges within one owning component; retain edges between parent and nested components. |

### Component ownership

A package exposing a supported C++ `:api` target defines a component. Its
implementation targets, API variants, headers, and tests belong to that component.
Descendant packages inherit the nearest owning component unless they define
another supported API boundary.

A nested component remains a separate component even when it lives under
`internal/` or `detail/`. Those directories express restricted access, not shared
ownership.

For cycle detection, map each resolved dependency's endpoints to their owning
components. Discard edges whose endpoints have the same owner; retain edges
between parent and nested components. Do not subsequently collapse nested
components into their parents.

For example, `//document:api`, `//document:test_api`, and tests in
`document/tests/` belong to `document`. The API and tests under
`document/internal/gap_buffer/` belong to that nested component. A dependency
from `document` to `document/internal/gap_buffer` is ordinary composition; adding
the reverse dependency creates a component cycle.

Use header-layout evidence where ownership is unambiguous. If supported boundary
indicators conflict or ownership cannot be established, report the affected check
as **not checked**.

### Supported inspection

Keep these checks syntactic and conservative about accusations:

- Infer only the C++ component/header boundaries needed by these rules, using
  `:api`, `include/<component>/`, package-root APIs, and nested internal layout.
  There is no need to assign every script, utility target, or resource an owner.
- Obtain structured facts with loading-phase `bazel query`; never regex-parse BUILD
  files. Keep code `deps`/`implementation_deps` distinct from tool and data inputs.
  Use straightforward explicit grants as evidence of supported special access;
  do not implement package-group or symbolic-macro visibility semantics. When
  evidence is insufficient, mark that check not checked and let Bazel enforce
  the actual permission during builds.
- Resolve ordinary relative includes and existing simple include-prefix mappings.
  Computed includes, ambiguous mappings, unavailable generated headers, and
  indirect inputs outside the supported subset are coverage limitations. Standard
  and external headers are outside first-party traversal, not missing coverage.
- Inspect source branches without evaluating the preprocessor. A path dependent
  on unresolved conditional combinations is a potential finding, not a definite
  violation. Do not fail on a cycle assembled from mutually exclusive `select()`
  choices or claim it was observed in a configured build.
- Extract include directives with a comment/string-aware scanner or a small
  parser. Do not build general C++ name lookup or type analysis. Add a pinned
  parser dependency only when a selected check actually needs it.

Explicit namespaced internal-type use and simple public aliases may be useful
later. They are outside the initial completion criteria. Arbitrary aliases,
templates, macro expansion, deduced types, semantic isolation, ABI checks, domain
roles, and configuration matrices are also outside this plan.

## Implementation steps

1. **Create the minimal foundation.** Create `tools/architecture/README.md` with
   the operating contract and four rules above, plus small private helpers as
   needed. Discover active tracked/untracked files with Git's NUL-delimited
   inventory, excluding ignored untracked output, deleted files, and symlinks
   without following linked directories. Keep the C++ sources/headers and active
   BUILD files needed by the checker. Derive packages from those BUILD files and
   query explicit `//package:*` patterns so ignored or linked directories are not
   recursively searched and manual/incompatible targets remain visible.

   Use the repository's pinned Bazel version and structured loading-phase output
   with selectors retained, for example `--output=streamed_jsonproto` and
   `--proto:flatten_selects=false`. Read only the facts needed for headers, source
   membership, code dependencies, test context, explicit grants, and simple
   include mappings. Keep conditional choices distinguishable from unconditional
   edges. No C++ toolchain analysis, saved fact inventory, comprehensive ownership
   model, or generic extraction framework is required. Add focused discovery and
   query fixtures, including an operational failure case.

2. **Implement the small checker.** Create `tools/architecture/check.py` as the
   public command, accepting `--repo` with a default relative to the script.
   Connect it to the new discovery/query helpers and infer only the boundaries
   needed by the selected rules. Implement literal include checks first, then
   direct dependency checks and unconditional cycles. Keep limitations explicit
   instead of adding a resolver for each unsupported case. Avoid a new generic
   policy framework or a comprehensive target taxonomy. Keep implementation and
   fixtures under `tools/architecture/` and document the test command in its
   README.

3. **Prove behavior with focused fixtures.** Give each rule passing and failing
   examples in a small repository with different component names. Cover comments
   and strings, include chains, parent/internal composition, selected test access,
   ordinary/test API variants, legitimate implementation grants, and conditional
   paths. Cover inherited ownership of tests and implementation targets, nested
   component ownership, passing parent-to-child composition, a failing cycle
   between parent and child, and discarded edges within one owner. Prove
   unsupported syntax is reported without failing, definite violations still fail
   alongside limitations, operational errors fail, and repeated runs produce the
   same findings. A conventional new component must need no registry edit.
   Preserve existing compiler-based public-header checks.

4. **Integrate and finish.** Run the checker and its focused tests from
   `tools/verify.py`, preserving invocation from another directory and aggregate
   failure reporting. Document exact supported forms, limitations, and commands.
   Keep Bazel build/default/debug tests as separate existing checks. Test that
   verification propagates checker failures. No aspects, validation-action wiring,
   mandatory checks on every targeted build, or new CI platform matrix are needed.

## Completion and verification

Done means the four supported rules work on their fixtures, current legitimate
repository arrangements pass, limitations are plainly reported, and the checker
runs through the existing verification entry point. Preserve private document
storage composition, selected internal test access, debug/test variants, POSIX
headers, and existing public-header assertions. Do not change product behavior
just to accommodate the checker.

Once implemented, run the documented focused checker tests and these commands:

```sh
python3 tools/architecture/check.py
python3 tools/verify.py
bazel build //:editor
bazel test //...
bazel test -c dbg //...
```

Add sophistication later only for a concrete violation worth detecting, with a
focused fixture and a clear benefit. Completion does not require complete source
coverage, full Bazel visibility resolution, or semantic C++ analysis.
