# Align with engineering guidance

Status: in progress; parts 1 and 2 and the first item of part 3 are implemented
and verified.
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
- [x] Add editor scenarios for significant navigation, scrolling, and
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

The first item of part 1 is complete. Every document contains one initial
logical line at byte zero; each newline adds another line. Default construction
now delegates to empty-vector construction. API tests cover both construction
paths, empty edits, insertion of text and newlines, deletion of all content,
and editing again after deletion. Logical-line iteration exposes one empty
view for an empty document. Acceptance scenarios and their executable bindings
are persisted in `features/empty_document.feature` and `document_tests.cpp`.

Validation: focused document and logical-line tests passed; `bazel test //...`
and `bazel test -c dbg //...` passed all six suites and built the editor.
Formatting checks passed for the changed header and test files, and
`git diff --check` passed. Existing GoogleTest signedness warnings remain
covered by the documented exception.

The second item of part 1 is complete. The document checker now verifies exact
agreement between newline bytes and line starts, including the initial start
at zero and absence of missing or extra entries. It reads through storage APIs
to avoid recursive document guards, allocates no temporary index, and compiles
the full scan out under `NDEBUG`. Construction now guards document invariants
as well as edits and partial views. API tests exercise every replacement range
in short documents with leading, adjacent, and trailing newlines, checking
content, line starts, and invariants after each edit. No private state or
test-only access was added.

Validation for item 2: focused document tests and all six suites in default
and debug builds passed. `bazel build -c opt //:editor` passed with the scan
disabled. Formatting checks for changed code and `git diff --check` passed.

The third item of part 1 is complete. `kMaxDocumentBytes` is the smaller of
the 32-bit byte-count maximum and iterator difference-type maximum, minus one.
The reserved value accommodates the initial logical line and exclusive
line-range end even for all-newline content. A compile-time check ties this
limit to the document line-number type. Physical gap storage may use the
reserved byte, but spare capacity growth is capped to keep all offsets
representable.

Always-on checked arithmetic in the private `internal/size_limits.h` rejects
oversized counts before narrowing and checks addition by subtraction.
Oversized construction and edits throw `std::length_error`; edits check the
post-deletion size before mutation. Line-index updates validate shifted starts,
new starts, line counts, and deletion ends before mutation. Their `noexcept`
specifications were removed so size-limit errors can propagate. General
allocation/edit failure guarantees remain a later item of part 1.

File loading rejects oversized files before allocating or narrowing their
length. The scenario in `features/document_size.feature` is bound to a sparse
file test through `gap_loader::Load`. Arithmetic boundary tests exercise actual
limits without allocating huge buffers; line-index API tests exercise offsets
at and beyond the supported limit.

Validation for item 3: `bazel test //...` and `bazel test -c dbg //...` passed
all eight suites. The size-limit and loader suites and the four new line-index
boundary tests also passed against optimized libraries with contracts disabled.
`bazel build -c opt //:editor`, formatting checks for changed code, and
`git diff --check` passed. Existing GoogleTest signedness warnings remain covered
by the documented exception.

The fourth item of part 1 is complete. Wrapped-row counting rounds up without
adding width to content length. Compile-time checks cover maximum byte counts,
width one, width two, maximum width, and empty content, and ensure the row-index
type can represent every possible count. API tests verify short-line content
and iteration at the two largest widths, empty documents at maximum width,
and exact-width multiples without an extra empty row. The wide-viewport
acceptance scenario is persisted in `features/wrapped_rows.feature` and bound
in `wrapped_row_range_tests.cpp`.

Validation for item 4: focused wrapped-row tests and all eight suites in default
and debug builds passed. Optimized wrapped-row behavior tests also passed
(excluding the zero-width test, which requires enabled contracts).
Formatting checks for changed code and `git diff --check` passed.

The fifth item of part 1 is complete. Valid edits have the strong failure
guarantee: allocation and size-limit exceptions leave bytes, line queries,
and existing views unchanged. `Document::Edit` stages a copy of the line index,
updates it independently, replaces bytes through `GapBuffer::Replace`, and
commits the index with a nonthrowing move checked at compile time.
`GapBuffer::Replace` allocates only the growth not covered by deletion, before
modifying logical bytes. `LineStarts::UpdateOnInsert` reserves its final size
before shifting starts or inserting entries. Exceptions propagate normally;
the earlier size-limit change already removed the erroneous `noexcept`.

Tradeoff: edits with inserted bytes temporarily copy the line index, adding
memory and work proportional to the number of lines, while byte storage is
not copied. Pure deletions and no-op edits retain their allocation-free path.
The public contracts, implementation ordering, compile-time check, and
acceptance scenarios preserve the failure policy beyond this temporary plan.

`edit_failure_tests.cpp` is an isolated test executable with scoped allocation
failure injection. It discovers allocation points through the supported APIs,
checks unchanged state and successful retry for each failure, and verifies
retained document views plus allocation-free deletion/no-op behavior.
Standalone gap replacement and line-index insertion are also covered. No
production test hooks or private-state access were added. Document scenarios
are persisted in `features/document_edit_failures.feature` with test bindings.

Validation for item 5: focused editing/failure tests and all nine suites in
default and debug builds passed. `bazel test -c opt //:edit_failure_tests`
passed against optimized libraries with contracts disabled, and
`bazel build -c opt //:editor` passed. The test allocator is compiled separately
from its clients to avoid GCC inlining-related new/delete warnings; no warning
exceptions were added. Formatting checks for changed code, scenario bindings,
and `git diff --check` passed.

The sixth item of part 1 is complete. `Key` uses encapsulated construction:
text and typed special-key factories are the only construction paths, all state
is private, and the untyped category/code factory is private. Default and
aggregate construction are unavailable. Copy and assignment replace the whole
key; `WithMods` preserves identity while replacing modifiers. Existing UTF-8
encoding, empty text for special keys, and modifier-subset matching are preserved.
Queries and modifier replacement are constexpr, retaining compile-time decoder
tables and allowing compile-time behavioral checks without additional machinery.

`key_tests.cpp` checks construction restrictions at compile time and exercises
UTF-8 encoding boundaries, special-category separation, modifier replacement,
and assignment between text and special keys through the supported API.
A small `:key` library lets tests depend only on key behavior; terminal support
and the editor depend on it explicitly. No input acquisition or command dispatch
was changed. These are type/state guarantees, so no Gherkin scenario was added.
The focused tests accumulated across part 1 also complete its final testing item.

Validation for item 6: focused key tests and all ten suites in default and debug
builds passed. `bazel build //:editor`, formatting checks for changed C++ files,
and `git diff --check` passed.

Part 2 is complete. `//editor_core:api` exposes `unplugged::Editor`, which owns
one document and its cursor, preferred column, and viewport. Commands are applied
through `HandleKey`; queries expose only read-only document access, the cursor
byte offset, and the viewport's top wrapped row. `CreateFrame` is a const query
returning owned row strings and one-based terminal cursor coordinates. Snapshots
remain valid after later edits. Layout row metadata and navigation helpers are
private. The component depends on the document and key libraries, with no file,
stdin, terminal, or raw-mode dependency. Bazel visibility restricts the new API
to its application consumer; tests depend on `:api`.

The application retains file loading, stdin acquisition, raw-mode setup, terminal
row-difference caching/output, and shutdown. Editing dispatch, navigation,
wrapping, gutter/filler construction, and cursor positioning moved into the
component. A `false` result from `HandleKey` preserves Alt+Escape exit handling.
The component owns layout calculation, so the document remains independent of
presentation. The shared-layout cache proposal records the new location and API;
cache implementation, document sharing, and resizing remain future work.

Behavior was preserved, including byte-wise insertion/deletion/navigation,
Home/End on logical rather than wrapped lines, modifier-subset command matching,
one-row page overlap, and selecting row starts at vertical viewport boundaries.
Up at the first document row and Down at the final full viewport row also retain
the existing row-start selection behavior. Tests record these cases rather than
silently changing them during extraction. Full-width EOF cursor coordinates
remain clamped to the last visible text cell. The private vertical direction is
typed and row arithmetic uses size_t rather than narrowing through int.

The new constructor accepts viewport dimensions for deterministic testing.
Zero height and unrepresentable text widths throw `std::invalid_argument`;
widths below five preserve the four-cell gutter plus minimum one-cell text area.
The application continues using its existing 80-by-25 viewport. No terminal
adapter behavior was changed.

Nineteen API tests cover commands, empty-document and EOF boundaries, UTF-8 byte
behavior, preferred-column restoration/reset, scrolling, wrapping, normalization
after edits, owned snapshots, exit, and viewport dimensions. Seven significant
editing/navigation scenarios are persisted under `editor_core/features/`, with
unique executable bindings in `editor_core/editor_tests.cpp`.

Validation for part 2: focused editor tests and all eleven suites in default
and debug builds passed. Optimized editor API tests passed against production
libraries with contracts disabled, and the optimized application built. A PTY
smoke test verified initial rendering, text insertion, Enter, Alt+Escape exit,
and restoration of terminal attributes on normal exit. Formatting checks,
scenario binding checks, and `git diff --check` passed. The extraction and plan
updates were committed as `b3f29c6` after user approval.

The first item of part 3 is complete. `//key_decoder:api` exposes a stateless
`DecodeKey(ByteSource&)` function and a small byte-acquisition port. `ByteReadResult`
is a discriminated byte-or-status value; NUL is a byte and timeout, EOF, and
acquisition error are separate outcomes. The decoder depends only on `:key`
and the standard library. Protocol tables and helpers have local linkage.
`read_key.cpp` implements the port with POSIX stdin reads and delegates decoding;
its supported `ReadKey()` signature is unchanged. No decoder test needs stdin,
a terminal, sleeping, or protocol negotiation.

This increment preserves existing parsing and consumption behavior. Escape plus
a timeout produces bare Escape; Escape plus EOF/error produces no key. Unsupported
or interrupted sequences produce no key and discard their consumed prefixes.
Legacy controls, Alt uppercase normalization, UTF-8, CSI, SS3, kitty mappings,
modifier fields, and optional event suffix handling moved without protocol-policy
changes. Parser limitations remain: UTF-8 scalar validity is not fully checked,
legacy Alt fallback supports only two/three-byte UTF-8, the event suffix is ignored,
and the existing uint8_t modifier parser cannot represent the protocol field 256.
These issues are separate behavior decisions rather than extraction changes.
Error details and terminal system-call handling remain later work in part 3.

Fifteen deterministic API tests cover supported input forms, normalization,
Unicode text, high modifier bits, special keys, NUL, Escape timing, initial
non-byte outcomes, interrupted sequences, unsupported/malformed input, and
subsequent keys after discarded prefixes. Three significant input scenarios
are persisted in `key_decoder/features/decoding.feature` and uniquely bound in
`key_decoder/decoder_tests.cpp`. Test assertions use only the decoded key API;
no implementation headers or test hooks were added. Repository guidance records
the package and focused-test command.

Validation for the decoder extraction: focused tests and all twelve suites in
default and debug builds passed. Optimized decoder tests and the editor build
passed. A PTY smoke test verified rendering, insertion, Enter, Alt+Escape exit,
and normal terminal restoration through the stdin adapter. Formatting checks,
scenario binding checks, and `git diff --check` passed. Decoder changes remain
uncommitted for user review.

Continue with scoped terminal ownership/restoration, the second item of part 3.
Do not overwrite unrelated work. Coordinate layout changes with the separate
[shared wrapped-row layout cache plan](shared-wrapped-row-layout-cache.md).
