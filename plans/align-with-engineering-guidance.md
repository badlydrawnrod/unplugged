# Align with engineering guidance

Status: planned; implementation has not started.
Created: 2026-10-06.

## Purpose and lifecycle

Align the editor with the engineering principles in the root `AGENTS.md`,
particularly explicit component boundaries, deterministic behavioral tests,
persisted acceptance behavior, and mechanically enforced invariants.

This is a temporary implementation plan. Update its checkboxes and record
relevant decisions as work proceeds. Commit it alongside the changes, then
remove it in the final cleanup commit. Preserve enduring intent in code,
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

- [ ] Define and implement a consistent empty-document contract.
  `Document{}` currently has zero lines, while construction from an empty
  vector has one. Cover subsequent edits as well as construction, and update
  existing tests to reflect the chosen contract.
- [ ] Strengthen document invariants to verify that the line index agrees
  with the document's newline bytes. The current checker only validates the
  storage and line index separately. Keep expensive checks appropriate to
  their build mode.
- [ ] Enforce supported document-size limits before narrowing sizes to
  32-bit `ByteIndex` and `ByteCount`. Check growth and offset arithmetic too.
- [ ] Make wrapped-row counting safe against integer overflow.
- [ ] Define edit failure guarantees. Review allocation-capable
  `LineStarts::UpdateOnInsert`, which is declared `noexcept` in production,
  and the sequential storage/index mutations in `Document::Edit`. Ensure
  the implementation matches the intended failure policy.
- [ ] Prevent contradictory text/special-key states in `Key`, using
  encapsulated construction or a discriminated representation. Keep the
  solution proportionate to current requirements.
- [ ] Add focused behavioral and boundary tests for these changes, using
  compile-time checks and assertions where they provide stronger guarantees.

Relevant files: `document.{h,cpp}`, `line_starts.{h,cpp}`, `gap_buffer.{h,cpp}`,
`types.h`, `wrapped_row_range.cpp`, `key.{h,cpp}`, and their test suites.

### 2. A testable editor component

- [ ] Extract editing commands, navigation, viewport state, and frame
  construction from `editor.cpp` behind a narrow supported API.
- [ ] Leave the application entry point responsible for connecting input,
  terminal output, file loading, and the editor component.
- [ ] Add deterministic API-level tests for insertion, deletion, Backspace,
  line/document navigation, preferred-column behavior, scrolling, wrapping,
  and cursor visibility. Define expected edge-case behavior explicitly.
- [ ] Preserve existing intended behavior during extraction. Separate
  intentional behavior fixes from mechanical movement where practical.

### 3. Terminal adapters and resource ownership

- [ ] Separate byte decoding in `read_key.cpp` from stdin acquisition, so
  input-protocol behavior can be tested with deterministic byte streams.
- [ ] Replace global saved terminal state and `atexit` handling in
  `raw_mode.cpp` with scoped terminal ownership and restoration.
- [ ] Check terminal system-call failures and define their handling.
- [ ] Separate input-protocol negotiation from raw-mode setup; remove the
  existing probe/debug-output hack as part of implementing that boundary.
- [ ] Make terminal output testable through a suitably small adapter where
  needed to verify rendering behavior.

Avoid introducing a general plugin or dependency-injection framework.

### 4. Persist significant acceptance behavior

- [ ] Add deterministic `.feature` scenarios under the owning component's
  `features/` directory for significant document editing behavior, including
  replacement and newline handling.
- [ ] Add editor scenarios for significant navigation, scrolling, and
  editing commands after the editor API exists.
- [ ] Bind every persisted scenario to executable tests through the
  supported component API, keeping the relationship traceable.
- [ ] Reuse existing tests where they actually implement the scenario.
  `BackspaceOnEmptyLineDeletesOnlyOneNewline` currently exercises document
  edits; it does not verify Backspace command dispatch or cursor movement.
- [ ] Use ordinary GoogleTest bindings unless a dedicated BDD runner has
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

- [ ] Establish supported document and editor targets reflecting their
  actual APIs and dependency direction.
- [ ] Give internal storage subcomponents their own `:api` targets and
  restrict implementation targets to legitimate consumers.
- [ ] Separate supported API headers from implementation headers. The
  current `DOCUMENT_MODEL_HDRS` exports document APIs, storage primitives,
  and contract machinery together.
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

## Resume notes

No implementation work has been performed under this plan. Begin by reading
the current `AGENTS.md`, checking the working tree, and confirming the relevant
contracts and baseline before starting item 1. Do not overwrite unrelated
work. Coordinate layout changes with the separate
[shared wrapped-row layout cache plan](shared-wrapped-row-layout-cache.md).
