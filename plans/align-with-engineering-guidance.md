# Align with engineering guidance

Status: in progress; parts 1–5, naming, and reviewed-file formatting in part 6
are implemented and verified. The mechanical formatting check is next.
Created: 2026-10-06.

## Purpose and lifecycle

Align the editor with the engineering principles in the root `AGENTS.md`,
particularly explicit component boundaries, deterministic behavioral tests,
persisted acceptance behavior, and mechanically enforced invariants.

This is a temporary implementation plan. Update its checkboxes and record
relevant decisions as work proceeds. Prepare changes for user review and commit
them alongside the plan only after approval, then remove it in the final cleanup
commit. Preserve enduring intent in code,
tests, acceptance scenarios, or `AGENTS.md` before removing the plan.

## Starting point

The repository already has small document primitives, behavioral GoogleTest
suites, private Bazel package visibility, standard algorithms, runtime
contracts, and compile-time range checks. Preserve these strengths.

The review baseline passed all six suites with:

```sh
bazel test //...
bazel test -c dbg //...
```

The default results were cached; the debug suites executed. GoogleTest
signedness warnings are covered by the existing documented exception.

The principal gaps are aggregate build targets, editor behavior embedded in
the terminal loop, missing acceptance scenarios, and incomplete state and
invariant guarantees. The items below are findings and proposed changes;
confirm the relevant contracts before changing behavior.

## Implementation sequence

Work in small, coherent increments. Keep the build and tests green after
each increment, and update this plan with completed work and remaining issues.

### 1. Correctness and state guarantees

- [x] Define and implement a consistent empty-document contract.
  Both default and empty-vector construction now yield one empty logical
  line at byte zero. Tests cover subsequent edits, including deletion of
  all content and insertion into the resulting empty document.
- [x] Strengthen document invariants to verify that the line index agrees
  with the document's newline bytes. The checker verifies the initial start
  at zero, a matching start after every newline, and no extra entries.
  The allocation-free full scan is compiled out when `NDEBUG` is defined.
- [x] Enforce supported document-size limits before narrowing sizes to
  32-bit `ByteIndex` and `ByteCount`. Construction, edits, line-index offsets,
  physical gap capacity, and file loading now enforce always-on limits.
- [x] Make wrapped-row counting safe against integer overflow.
  Nonempty content uses `1 + (content_count - 1) / width`, avoiding the
  overflowing rounding addition. Empty content still produces one row.
- [x] Define edit failure guarantees. Allocation and size-limit errors
  propagate without changing document bytes or line queries. Byte replacement
  finishes allocation before mutation, and a prepared line index is committed
  with a compile-time-verified nonthrowing move. Standalone line-index insertion
  reserves before mutation. Deletion and no-op edits remain allocation-free.
- [x] Prevent contradictory text/special-key states in `Key`, using
  encapsulated construction or a discriminated representation. Keep the
  solution proportionate to current requirements.
- [x] Add focused behavioral and boundary tests for these changes, using
  compile-time checks and assertions where they provide stronger guarantees.

Relevant files: `document.{h,cpp}`, `line_starts.{h,cpp}`, `gap_buffer.{h,cpp}`,
`types.h`, `wrapped_row_range.cpp`, `key.{h,cpp}`, and their test suites.

### 2. A testable editor component

- [x] Extract editing commands, navigation, viewport state, and frame
  construction from `editor.cpp` behind a narrow supported API.
- [x] Leave the application entry point responsible for connecting input,
  terminal output, file loading, and the editor component.
- [x] Add deterministic API-level tests for insertion, deletion, Backspace,
  line/document navigation, preferred-column behavior, scrolling, wrapping,
  and cursor visibility. Define expected edge-case behavior explicitly.
- [x] Preserve existing intended behavior during extraction. Separate
  intentional behavior fixes from mechanical movement where practical.

### 3. Terminal adapters and resource ownership

- [x] Separate byte decoding in `read_key.cpp` from stdin acquisition, so
  input-protocol behavior can be tested with deterministic byte streams.
- [x] Replace global saved terminal state and `atexit` handling in
  `raw_mode.cpp` with scoped terminal ownership and restoration.
- [x] Check terminal system-call failures and define their handling.
- [x] Separate input-protocol negotiation from raw-mode setup; remove the
  existing probe/debug-output hack as part of implementing that boundary.
- [x] Make terminal output testable through a suitably small adapter where
  needed to verify rendering behavior.

Avoid introducing a general plugin or dependency-injection framework.

### 4. Persist significant acceptance behavior

- [x] Add deterministic `.feature` scenarios under the owning component's
  `features/` directory for significant document editing behavior, including
  replacement and newline handling.
- [x] Add editor scenarios for significant navigation, scrolling, and
  editing commands after the editor API exists.
- [x] Bind every persisted scenario to executable tests through the
  supported component API, keeping the relationship traceable.
- [x] Reuse existing tests where they actually implement the scenario.
  Document newline deletion is named and bound as a byte edit; Backspace
  dispatch and cursor movement are verified through `//editor_core:api`.
- [x] Use ordinary GoogleTest bindings unless a dedicated BDD runner has
  clear value. Keep storage implementation properties out of Gherkin.

For traceability, place these annotations immediately above each binding:

```cpp
// Feature: <repo-relative-path-to-feature-file>
// Scenario: <exact scenario title>
TEST(...) {
  ...
}
```

### 5. Enforce component boundaries in Bazel

- [x] Establish supported document and editor targets reflecting their
  actual APIs and dependency direction. `//document:api` owns the model and
  views in `document/`; `//editor_core:api` and the application depend on that
  supported target. Document API tests and scenarios moved with the component.
  `//document:test_api` preserves the existing test contract configuration,
  inherited from the matching storage variant across translation units.
- [x] Give internal storage subcomponents their own `:api` targets and
  restrict implementation targets to legitimate consumers. Gap buffer and line
  index now live in `document/internal/gap_buffer/` and
  `document/internal/line_starts/`. Supported aliases expose package-private
  implementation targets. Production line-index access is document-only; gap
  buffer access also permits the root file loader. Tests moved beside storage
  and depend on supported `:test_api` variants.
- [x] Separate supported API headers from implementation headers. Document
  headers use unsupported concrete declarations in `document/detail/` for
  inline ownership, without exposing the internal storage API aliases.
  Storage API headers remain at their subcomponent roots. Contract checks
  moved to `contracts/impl/checks.h`; only invariant diagnostic types appear
  in supported headers. Compile-time boundary checks reject exported storage
  API aliases or contract macros and preserve document copy/move guarantees.
- [x] Make boundary tests depend on the supported component target and
  storage tests on their supported subcomponent targets. Document boundary
  tests use `:test_api`, storage tests use their matching `:test_api` aliases,
  and no behavioral test depends on layout or contract-check implementations.
  The mixed allocation-failure suite explicitly uses all three supported APIs.
  Size-limit implementation tests use their focused helper target.
- [x] Replace `LineStarts::LineNumber` in document and logical-line public
  contracts with an appropriately owned document-domain type. Both alias
  `unplugged::document::LineNumber`, owned by `document/line_number.h`.
- [x] Keep implementation helpers local; review helpers such as
  `FindLineStarts` for unnecessary external linkage. The document constructor's
  helper now has internal linkage through an anonymous namespace. Other
  source-local helpers already use anonymous namespaces; shared header helpers
  remain inline/constexpr or templates behind their implementation targets.
- [x] Preserve the existing contract-test configuration coherently across
  translation units while splitting targets. `//:contract_test_mode`
  propagates the debug throwing-contract policy to both storage test APIs,
  document implementation, and their consumers.

The current private package visibility is a useful starting point, but does
not distinguish consumers within the root package. Introduce packages and
directories where needed to enforce meaningful boundaries. Concrete private
members alone do not justify PImpl or additional allocation.

Update repository-specific paths and commands in `AGENTS.md` if files or
targets move. Its structural description should remain accurate.

### 6. Naming, formatting, and mechanical checks

- [x] Apply the naming conventions in `AGENTS.md` consistently to custom
  APIs, including `check_invariants`, `line_number`, and `row_count`.
  Invariant checks now use `CheckInvariants`; logical-line queries use
  `GetLineNumber`, `FirstLine`, `EndLineExclusive`, and `NumLines`; wrapped-row
  queries use `GetRowIndex`, `Width`, and `NumRows`. The iterator queries use
  `Get...` to avoid colliding with their type aliases. Contract helper functions,
  macros, diagnostics, and all API consumers use the renamed functions.
- [x] Rename camelCase locals such as `gapSize` to snake_case, and constants
  such as `InitialGapSize` to `kPascalCase`. Gap-buffer implementation locals and
  test locals/parameters now use snake_case. Randomized-test settings use
  `kDefaultSeed`, `kSeedsToRun`, and `kOpsPerSeed`; production constants,
  including `kInitialGapSize`, already conform. Archived experiments remain
  untouched.
- [x] Preserve standard interoperability names such as `begin`, `end`,
  and iterator traits. Standard range queries `size` and `empty`, iterator type
  names, operators, and existing range/iterator compile-time checks remain
  unchanged.
- [x] Format the files identified by the review:
  `document/document.cpp`, `editor.cpp`, `document/logical_line_range.{h,cpp}`,
  `document/wrapped_row_range.{h,cpp}`, and
  `document/wrapped_row_range_tests.cpp`. Applied clang-format 21.1.8 with the
  repository configuration; `editor.cpp` already conformed. All seven files
  pass `clang-format --dry-run --Werror`.
- [ ] Add a mechanical formatting check using the repository's
  `.clang-format`. Keep generated build files out of source changes.

Keep mechanical renaming and formatting separate from behavior changes
where practical, so review remains straightforward.

## Verification and completion

- [ ] Run focused tests for each changed component during implementation.
- [ ] Run `bazel test //...` and `bazel test -c dbg //...` after each coherent
  increment. Adapt target paths if the build structure changes.
- [ ] Build `//:editor` and smoke-test the terminal adapter changes in an
  interactive terminal, including normal exit and restoration after failure.
- [ ] Check scenario bindings, dependency visibility, formatting, and
  `git diff --check` before completing the work.
- [ ] Confirm acceptance tests verify intended behavior and would survive
  replacement of the implementation behind the supported APIs.
- [ ] Record enduring decisions in their owning artifacts and remove this
  temporary plan in the final cleanup commit.

## Remaining implementation context

Protocol setup follows the kitty quickstart: push escape-code disambiguation
at startup and pop at teardown, without querying support or consuming stdin.
`terminal::InputProtocol` owns this scope independently of raw mode; normal
restoration reports failures and destruction cleans up best effort. Terminals
that ignore these requests continue using legacy decoding. API tests and
`features/input_protocol.feature` cover setup and cleanup behavior. All seventeen
suites pass in default and debug builds. PTY checks cover legacy/kitty input,
startup without replies, early input, normal restoration, read errors, and
broken output pipes; formatting and scenario bindings also pass.

Next: add a mechanical formatting check using the repository configuration,
keeping generated build files outside source changes. Supported document headers now include only
supported diagnostic/domain types and unsupported `detail/`
representation declarations. The detail declarations preserve inline storage
and the existing copy/move behavior, without adding allocation or indirection.
Internal storage APIs expose aliases to those concrete types for their own
consumers; document public contracts do not expose the aliases.

The transitional support aggregate is gone. `//:document_types`,
`//:size_limits`, `//contracts:api`, and the restricted `//contracts:checks`
express separate responsibilities. `//:contract_test_mode` consistently
propagates debug throwing contracts across document/storage test implementations
and their consumers. The mixed allocation-failure suite remains in the root,
using supported document and storage test APIs. API tests no longer include
contract-check machinery.

The header-separation increment passed nine focused suites, all seventeen
suites in default and debug configurations, and `bazel build //:editor`.
Compile-time boundary checks
verify that supported document headers expose neither storage API aliases nor
contract macros. Header-include and Bazel visibility checks, scenario bindings,
formatting of new headers, and `git diff --check` also passed. No document,
editor, or terminal behavior changed.

The final part-5 increment gives `FindLineStarts` internal linkage and confirms
that other source-local helpers already remain local. Symbol inspection shows
a local (`t`) anonymous-namespace symbol, with no global definition. Five focused
document suites, all seventeen suites in default and debug configurations, and
`bazel build //:editor` passed. Scenario bindings and `git diff --check` also
passed. This changes linkage only; document behavior and supported APIs remain
unchanged.

The API-naming increment renames custom queries and contract helpers together.
Only function identifiers and their documentation changed; standard range
interoperability names and behavioral expectations remain unchanged. Eight
focused suites, all seventeen suites in default and debug configurations, and
`bazel build //:editor` passed. A mechanical comparison confirms that C++ changes
are exactly the intended name substitutions; searches find no old function
names in active code. All 42 scenario bindings and `git diff --check` passed.

The local/constant naming increment changes 26 identifiers in the gap-buffer
implementation and tests. A mechanical comparison confirms exact name
substitutions without changes to operations, constants' values, or expectations.
The two focused suites, all seventeen suites in default and debug configurations,
and `bazel build //:editor` passed. Active code has no remaining camelCase
local/parameter identifiers or nonconforming constexpr variable names. Scenario
bindings and `git diff --check` passed. Formatting remains a separate step.

The reviewed-file formatting increment applies the repository's clang-format
configuration to all seven listed files. A mechanical comparison confirms that
each file exactly matches formatter output from its committed version. Six
files changed; the application entry point already conformed. Six focused
suites, all seventeen suites in default and debug configurations, and
`bazel build //:editor` passed. The formatter's dry-run check, all 42 scenario
bindings, and `git diff --check` passed. No behavior or component boundaries
changed. Repository-wide formatting enforcement remains the next step.

The decoder extraction preserved existing parsing behavior. UTF-8 scalar
validity is not fully checked, legacy Alt fallback supports only two/three-byte
UTF-8, the event suffix is ignored, and the `uint8_t` modifier parser cannot
represent protocol field 256. Treat fixes to these limitations as explicit
behavior changes rather than incidental negotiation refactoring.

Coordinate layout changes with the separate
[shared wrapped-row layout cache plan](shared-wrapped-row-layout-cache.md).
