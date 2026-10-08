# Align with engineering guidance

Status: in progress; parts 1–4 are implemented and verified. Part 5 has
started: document/editor API targets are established; storage subcomponent
boundaries are next.
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
- [ ] Give internal storage subcomponents their own `:api` targets and
  restrict implementation targets to legitimate consumers.
- [ ] Separate supported API headers from implementation headers. The
  document API still transitively exports storage primitives and contract
  machinery through the transitional storage aggregate.
- [ ] Make boundary tests depend on the supported component target and
  storage tests on their supported subcomponent targets. Reserve direct
  implementation dependencies for tests of implementation properties.
- [ ] Replace `LineStarts::LineNumber` in document and logical-line public
  contracts with an appropriately owned document-domain type.
- [ ] Keep implementation helpers local; review helpers such as
  `FindLineStarts` for unnecessary external linkage.
- [ ] Preserve the existing contract-test configuration coherently across
  translation units while splitting targets.

The current private package visibility is a useful starting point, but does
not distinguish consumers within the root package. Introduce packages and
directories where needed to enforce meaningful boundaries. Concrete private
members alone do not justify PImpl or additional allocation.

Update repository-specific paths and commands in `AGENTS.md` if files or
targets move. Its structural description should remain accurate.

### 6. Naming, formatting, and mechanical checks

- [ ] Apply the naming conventions in `AGENTS.md` consistently to custom
  APIs, including `check_invariants`, `line_number`, and `row_count`.
  Update the contract machinery and all consumers alongside those renames.
- [ ] Rename camelCase locals such as `gapSize` to snake_case, and constants
  such as `InitialGapSize` to `kPascalCase`.
- [ ] Preserve standard interoperability names such as `begin`, `end`,
  and iterator traits.
- [ ] Format the files identified by the review:
  `document.cpp`, `editor.cpp`, `logical_line_range.{h,cpp}`,
  `wrapped_row_range.{h,cpp}`, and `wrapped_row_range_tests.cpp`.
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

Next: give storage subcomponents their own `:api` targets in part 5, then
separate supported headers from implementation and contract machinery. The
root `//:document_storage` and test variant are transitional aggregates,
restricted to root/document consumers; editor consumers use `//document:api`.
Document boundary tests use `//document:test_api` with the same supported
headers. The mixed allocation-failure suite remains in the root and explicitly
depends on both document and storage APIs until storage tests are separated.
This increment passed nine focused suites, all seventeen suites in default and
debug configurations, and `bazel build //:editor`. Scenario bindings and
`git diff --check` also passed. Moved C++ files changed only include paths and
scenario annotations.

The decoder extraction preserved existing parsing behavior. UTF-8 scalar
validity is not fully checked, legacy Alt fallback supports only two/three-byte
UTF-8, the event suffix is ignored, and the `uint8_t` modifier parser cannot
represent protocol field 256. Treat fixes to these limitations as explicit
behavior changes rather than incidental negotiation refactoring.

Coordinate layout changes with the separate
[shared wrapped-row layout cache plan](shared-wrapped-row-layout-cache.md).
