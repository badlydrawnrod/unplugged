# Align repository structure with component ownership

Status: steps 1–2 complete; steps 3–7 remain proposed.
Created: 2026-10-08.

## Objective and scope

Align this repository with the component boundaries, ownership, layout, ports,
and acceptance validation described in `Repo Structure Explained.md` in the
adjacent `_metaproject` repository. This plan captures the relevant requirements
so implementation does not depend on that external file remaining available.

Prioritize enforceable architectural boundaries before filesystem consistency.
Preserve existing editor behavior, supported contracts, deterministic tests,
inline document ownership, and the coherent debug contract configuration.

This work includes:

- Removing external access to document storage subcomponents.
- Persisting acceptance traceability validation and correcting feature ownership.
- Separating application composition from components and adapters.
- Giving repository-visible components distinct API, source, and test directories.
- Making the existing decoder-owned byte-source port explicit.
- Preserving durable contracts in their owning components.

It excludes decoder behavior fixes, layout-cache implementation, new terminal
features, and general dependency-injection infrastructure. Coordinate paths and
dependencies with [the layout-cache proposal](shared-wrapped-row-layout-cache.md)
without implementing that proposal here.

Keep this plan updated as increments complete. Prepare each increment for review;
commit only when instructed. At completion, preserve enduring decisions in code,
tests, component specifications, and `AGENTS.md`, then remove this temporary plan
in an authorized cleanup commit.

## Current evidence and decisions

- Document, editor, decoder, and terminal I/O have supported `:api` targets.
  Component tests use those boundaries or their documented `:test_api` variants.
- Storage production and test APIs are restricted to the owning document package
  and local tests. The loader exposes only `Load()` and depends directly on
  `//document/types:api`; unused `From()` and storage includes have been removed.
- Allocation-failure suites live in document and each storage package and consume
  their supported test APIs. Shared injection and failure discovery use the
  narrowly visible, test-only `//test_support:allocation_failure` target.
- There are 42 persisted scenarios with 42 valid executable bindings. The
  persisted checker in `tools/acceptance/` validates at least one binding per
  scenario; it permits multiple bindings and documents supported syntax.
- Root `features/empty_document.feature` and `features/wrapped_rows.feature`
  describe document behavior. The oversized-file scenario lives under document
  but is bound to the file loader's API.
- The root Bazel package combines key types, loading, terminal adapters,
  application composition, test support, and document helpers. Package visibility
  cannot distinguish these responsibilities.
- `ByteSource` is already owned by the decoder. `FdByteSource` implements it in
  `read_key.cpp`, and decoder tests provide a deterministic stream. Dependency
  direction is correct; port layout and independent conformance checks are absent.
- Public document headers include unsupported concrete `detail/` declarations
  to retain inline ownership. Preserve this documented extension of the guide's
  header-machinery convention. Do not introduce PImpl or allocation for layout
  uniformity. Consumers must not depend on those layout targets directly.

## Intended structure

Repository-visible components use:

```text
<component>/
  BUILD.bazel
  include/<component>/*.h
  src/*.cpp
  tests/
    BUILD.bazel
    *_tests.cpp
  features/
  docs/specs/                    # only where prose adds durable intent
  ports/<name>/                 # only for an actual required contract
  internal/<name>/
    BUILD.bazel
    <name>.h
    <name>.cpp
    *_tests.cpp
    impl/                       # only where implementation details exist
```

External includes remain `<component>/<header>.h`, with Bazel include-prefix
configuration mapping physical `include/` paths to those logical paths.
Implementation headers are not exported as supported API. Keep flat API layouts
for internal subcomponents and ports; do not add recursively nested `internal/`
directories. Retain `BUILD.bazel` and existing `*_tests.cpp` naming.

Proposed ownership:

| Package | Responsibility and supported target |
| --- | --- |
| `applications/editor/` | Main loop, frame rendering, concrete composition; `:editor` |
| `document/` | Document model and views; `:api`, documented `:test_api` |
| `document/types/` | Lightweight byte types and supported size constants needed by document consumers; `:api` |
| `document/internal/gap_buffer/` | Private storage subcomponent; `:api`, `:test_api` |
| `document/internal/line_starts/` | Private storage subcomponent; `:api`, `:test_api` |
| `document/internal/size_limits/` | Checked arithmetic implementation helper, visible only to its document/storage consumers |
| `editor_core/` | Editing, navigation, viewport, frame snapshots; `:api` |
| `key/` | Shared key value types; `:api` |
| `key_decoder/` | Protocol decoding; `:api` |
| `key_decoder/ports/byte_source/` | Required byte/event acquisition contract; `:api` |
| `file_loader/` | File-to-byte loading and its size-limit behavior; `:api` |
| `terminal/` | Existing POSIX terminal lifetimes, output, protocol scope, key-input adapter; `:api` |
| `terminal_io/` | Checked descriptor operations; `:api` |
| `contracts/` | Supported invariant diagnostics; `:api`; restricted check implementation |
| `test_support/` | Test-only allocation injection and PTY helpers; narrowly visible targets |
| `tools/acceptance/` | Traceability checker and its focused tests |

The root may retain a compatibility alias `//:editor` for build/run commands.
Component consumers depend on supported targets, never on that application's
implementation. Apply the repository-visible layout to the provided components;
keep helper packages proportionate to their responsibilities.

## Implementation sequence

### 1. Close document storage boundaries

- [x] Remove unused `gap_loader::From()` and its internal storage include.
  Keep `Load()` behavior and its size-limit rejection unchanged.
- [x] Give the loader a direct dependency on the supported byte/size declarations
  it actually uses. Establish `//document/types:api` without exposing storage
  implementation. Avoid relying on incidental transitive includes.
- [x] Split the mixed failure suite: document failure scenarios belong with
  document API tests; gap-buffer and line-index failure tests belong in their
  owning storage packages and use their supported test APIs.
- [x] Place shared allocation injection in a test-only support target. Preserve
  global allocation overrides, required linker behavior, and deterministic
  failure discovery without granting access to component implementation targets.
- [x] Remove root visibility from both storage test APIs and the gap-buffer
  production API. Permit only the owning document package and local tests at
  this stage; later update exact visibility for the document tests package.
- [x] Update `AGENTS.md` and affected acceptance annotations for moved tests.

Acceptance: no consumer outside `document/` includes or depends on either storage
API; the loader still rejects oversized files; all failure guarantees remain
covered. Validate with dependency queries and a temporary forbidden-consumer
build that must fail visibility analysis, removing the probe afterward.

### 2. Persist acceptance traceability validation

- [x] Add `tools/acceptance/check_traceability.py`, runnable from any working
  directory and discovering repository files without following generated output.
- [x] Recognize the supported Gherkin/test forms actually used, including
  `TEST`, `TEST_F`, and `TEST_P`; require immediately adjacent feature/scenario
  annotations with exact titles and repository-relative paths.
- [x] Fail for unbound scenarios, nonexistent feature/scenario references,
  duplicate scenario identities, and malformed or detached annotations.
  Allow multiple executable bindings for one scenario, as the guide requires
  at least one. Do not enforce today's one-to-one count as a permanent contract.
- [x] Report feature/test paths and line numbers on failure. Document supported
  syntax explicitly; do not silently ignore unsupported Gherkin constructs.
- [x] Add focused checker tests using temporary fixtures for success, each
  rejection, parameterized bindings, and multiple bindings per scenario.
- [x] Add a repeatable repository verification entry point for traceability and
  formatting; document it in `AGENTS.md`. If CI is introduced, have it invoke
  these checks rather than duplicating their logic. Prefer existing tooling;
  justify any new runner dependency before adding it.

Acceptance: all current scenarios pass; intentionally broken fixture bindings
fail with useful diagnostics. Moves in subsequent increments must pass this
persisted checker.

### 3. Correct acceptance ownership and extract file loading

- [ ] Move empty-document and wrapped-row features under `document/features/`.
- [ ] Move the oversized-file feature and its executable test under a new
  `file_loader/` component with supported `:api` target.
- [ ] Move document allocation-failure bindings into the document test package
  if not already completed in step 1.
- [ ] Update every annotation and dependency together. Preserve scenario titles
  and observable expectations during mechanical moves.
- [ ] Give file loading the repository-visible API/source/test layout and direct
  dependencies on supported byte/size declarations only.

Acceptance: features are owned by the component API they exercise; the traceability
checker passes; application loading does not regain storage dependencies.

### 4. Separate composition, terminal adapters, and shared key types

- [ ] Extract shared key types to `//key:api` and update direct consumers.
- [ ] Move existing terminal lifetime/output/protocol/input-adapter facilities
  into `terminal/` behind a supported API, preserving their current contracts.
  Keep checked descriptor operations behind `//terminal_io:api`.
- [ ] Move their tests and features to the owning component. Preserve isolated
  PTY tests and focused syscall/signal wrapping tests with their link options.
- [ ] Move the main loop and application-specific rendering to
  `applications/editor/main.cpp`. Compose editor, file loading, and terminal
  facilities there; core components must not depend on the application.
- [ ] Keep `//:editor` as a compatibility alias if useful. Remove obsolete root
  implementation targets and update legitimate visibility lists explicitly.
- [ ] Move shared PTY test support into test-only helper packages and document
  their consumers. Do not create a broad production support aggregate.

Acceptance: root production code is reduced to genuine repository configuration
and optional compatibility aliases; each component has an identifiable supported
boundary. Smoke-test real PTY startup, editing, legacy/kitty exit, protocol cleanup,
and terminal restoration after a broken output pipe.

### 5. Make the decoder-owned port explicit

- [ ] Move byte/event result types and `ByteSource` into
  `key_decoder/ports/byte_source/byte_source.h` behind its own `:api` target.
  Keep parsing and `DecodeKey()` in the decoder component.
- [ ] Make both decoder implementation and terminal adapter depend directly on
  the port. The decoder must not depend on terminal or POSIX implementation.
- [ ] State byte ordering and Timeout/EOF/Error semantics, including how the
  existing FD adapter reports syscall failures as exceptions. Preserve existing
  behavior; do not normalize incompatible implementations by changing semantics
  incidentally during extraction.
- [ ] Add reusable conformance checks for the genuinely shared semantics and run
  them for the deterministic test stream and FD adapter where applicable. Keep
  timing, descriptor hangup, and syscall-specific expectations in adapter tests.
- [ ] Supply concrete adapters outside the decoder. Keep parser protocol/UTF-8
  limitations documented and unchanged.

Acceptance: dependency direction points toward the decoder-owned port; decoder
tests remain independent of OS I/O; shared conformance tests protect actual
contract promises rather than internal call sequences.

### 6. Apply component layouts in separate mechanical increments

- [ ] Migrate document public headers/sources/tests, preserving internal storage
  packages and inline `detail/` declarations with restricted layout targets.
- [ ] Migrate editor core, decoder, terminal I/O, and invariant diagnostics to
  their API/source/test layout. Complete remaining layout work for key, terminal,
  and file loader if not done during extraction.
- [ ] Use Bazel include-prefix configuration to preserve logical includes.
  Update internal include paths and direct dependencies deliberately.
- [ ] Update visibility for newly separate tests packages; behavioral tests use
  `:api` or the documented same-contract `:test_api` variant. Focused implementation
  tests retain only the implementation dependencies their properties require.
- [ ] Preserve `CONTRACT_EXCEPTIONS` coherence across translation units, including
  inline definitions and conditional `noexcept`. Keep public-header compile
  checks covering diagnostic/domain types, storage aliases, and contract macros.
- [ ] Keep compatibility aliases only where an actual consumer benefits. Update
  commands in `AGENTS.md` and paths in the layout-cache proposal.

Acceptance: physical structure communicates API, implementation, and test ownership;
logical consumer includes remain stable; no implementation visibility is widened
to compensate for a file move.

### 7. Preserve durable intent and finish

- [ ] Add concise component specifications only for contracts that need more
  explanation than API comments and scenarios provide. Cover document edit/view
  lifetimes and failure guarantees, editor byte navigation/wrapping behavior,
  terminal restoration/error policy, and byte-source semantics as needed.
- [ ] Ensure component documentation links to supported targets, API headers,
  features, and relevant verification commands. Record the inline `detail/`
  representation choice and test API configuration where owned.
- [ ] Update `AGENTS.md` to describe the final layout and mechanical checks.
- [ ] Audit all consumers and features against the final boundaries, remove
  obsolete targets/includes/references, and confirm no generated files are tracked.
- [ ] Preserve the layout-cache proposal's dependency direction and proposed
  status; structural alignment must not introduce presentation ownership into
  document storage.
- [ ] Remove this temporary plan after the completed work is reviewed and its
  enduring decisions are captured, in an authorized cleanup commit.

## Verification for every coherent increment

- Run focused tests for the moved or affected components.
- Run `bazel test //...` and `bazel test -c dbg //...`.
- Build `//:editor` or its retained compatibility alias.
- Run `python3 tools/check_format.py`, the persisted traceability checker once
  available, and `git diff --check`.
- Inspect Bazel direct dependencies and visibility after boundary changes.
  Do not rely solely on source searches or successful compilation to demonstrate
  that forbidden consumers are excluded.
- Run PTY smoke checks after application/terminal moves; compare saved terminal
  attributes after normal exit and failure. Documentation-only increments do not
  require repeating terminal checks.
- Record observed results and any deviations here. Keep structural movement
  separate from behavior changes, and stop adding complexity until the current
  increment is green.


### Step 1 results (2026-10-08)

- Moved lightweight byte/size declarations from root `types.h` to
  `document/types/types.h` behind `//document/types:api`; updated direct consumers
  and removed the obsolete `//:document_types` target. Checked arithmetic remains
  behind `//:size_limits` until its later layout increment.
- Split the existing four failure tests into three isolated executables named
  `:edit_failure_tests` in their owning packages. Preserved the document feature
  annotations, expectations, and allocation-point discovery without asserting
  allocation count or order. Shared global allocation overrides are always linked
  from the test-only support library, with static linking in each failure suite.
- Focused tests passed (10 targets), as did `bazel test //...` and
  `bazel test -c dbg //...` (19 targets each), and `bazel build //:editor`.
- `python3 tools/check_format.py` passed for 60 C++ files; `git diff --check`
  passed. A temporary traceability check confirmed all 42 scenarios retain
  immediately adjacent executable bindings. The persisted checker remains step 2.
- Direct reverse-dependency queries for both storage `:api` and `:test_api`
  targets returned only document and local storage consumers. Four temporary root
  consumers, one for each boundary target, all failed Bazel visibility analysis;
  the probes were removed. Source inspection found no storage API includes
  outside `document/`.
- Small tooling correction: the formatting checker now skips deleted cached Git
  paths so uncommitted file moves can pass the required check. It still checks
  active tracked and untracked sources and excludes ignored build output.

### Step 2 results (2026-10-08)

- Step 1 was committed as `295b4e8` (`Close document storage boundaries`).
- Added `tools/acceptance/check_traceability.py` with Git-based discovery of
  active tracked/untracked feature and C++ files, skipping deleted paths,
  ignored output, and symlinks. Invocation is independent of working directory.
- Documented the supported English Gherkin subset and GoogleTest definition
  forms in `tools/acceptance/README.md`. Unsupported constructs, duplicate
  identities, unbound scenarios, invalid references, and malformed/detached
  annotations fail with file/line diagnostics. Comments and string literals do
  not provide executable bindings; multiple bindings per scenario are allowed.
- Added 20 standard-library unittest tests with temporary fixtures, including
  parameterized and multiline test definitions, every rejection category,
  multiple bindings, discovery through file moves, ignored generated output,
  CLI failure diagnostics, and invocation from another directory.
- Added `python3 tools/verify.py` to run formatting, repository traceability,
  and focused checker tests, and documented it in `AGENTS.md`. No dependencies
  or CI runner were added. Python tooling caches are ignored.
- Verification passed: all 42 persisted scenarios and 42 current bindings;
  20 checker tests; formatting for 60 C++ files; `git diff --check`;
  `bazel test //...` and `bazel test -c dbg //...` (19 targets each);
  `bazel build //:editor`. No production behavior or Bazel boundaries changed.

## Completion criteria

- External document consumers cannot reach storage subcomponent APIs.
- Provided component APIs, required ports, internal subcomponents, and application
  composition have explicit ownership and appropriately restricted Bazel targets.
- Repository-visible components follow the documented API/source/test layout;
  internal packages remain flat and proportionate.
- Every persisted acceptance scenario belongs to its exercised component, has
  at least one executable binding, and passes a persisted traceability check.
- Existing behavior, failure guarantees, copy/move guarantees, decoder limitations,
  and terminal cleanup remain unchanged unless a separate behavior change is
  explicitly undertaken.
- Build, default/debug tests, mechanical checks, and relevant PTY smoke tests pass.
- Enduring intent remains understandable without this plan or the external guide.
