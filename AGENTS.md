These sections separate general engineering principles from repository-specific
instructions so they can be split into separate guidance files as the
repository grows. Engineering Guidance takes precedence if the sections
conflict.

# Engineering Guidance

## Preserve intent

Implement the requested behavior, not merely the requested code shape.

Prefer designs that make intent explicit, local, and mechanically verifiable.

Prefer mechanical enforcement over written guidance where practical.

## Components and boundaries

Design small, cohesive components with narrow, stable APIs and explicit dependency direction.

Treat architectural boundaries recursively. A component may contain subcomponents that expose stable APIs to their parent while remaining invisible outside it.

### Bazel boundaries

Use Bazel targets and `visibility` to express architectural boundaries.

For internal subcomponents:

- Use `:api` for the supported boundary target.
- Consumers depend on `:api`, not its implementation targets.
- Keep API headers at the subcomponent root.
- Keep implementation headers under `impl/`.
- Restrict implementation targets to their legitimate consumers.
- If consumers repeatedly need an implementation target, reconsider the boundary rather than widening visibility automatically.

Example:

```text
messaging/internal/spsc_queue/
  BUILD
  spsc_queue.h
  spsc_queue.cpp
  impl/
    ring_storage.h
    sequence.h
```

```text
API target:
  //messaging/internal/spsc_queue:api

Consumer:
  #include "messaging/internal/spsc_queue/spsc_queue.h"

Implementation target:
  //messaging/internal/spsc_queue:ring_storage

Implementation:
  #include "messaging/internal/spsc_queue/impl/ring_storage.h"
```

Do not expose `impl/` headers through an API header.

## Verification and testing

Use the strongest appropriate mechanism for expressing correctness, in this order:

1. Type-system and compile-time guarantees.
2. Runtime assertions for local invariants.
3. Behavioral tests through stable component APIs.
4. Focused implementation tests where implementation properties themselves matter.

Test each component or subcomponent through the narrowest stable API exposed to its consumers.

Boundary tests:

- depend on the supported component target, normally `:api`;
- verify externally observable behavior;
- remain unchanged when the implementation is replaced without changing the contract.

Implementation tests may depend directly on implementation targets when properties of the chosen implementation matter.

Avoid coupling boundary tests to internal call sequences, private structure, or incidental representation.

Do not expose implementation details merely to make testing easier.

## Acceptance behavior

Capture significant externally observable behavior as persisted Gherkin `.feature` scenarios.

- Store feature files under the owning component's `features/` directory.
- Keep scenarios behavioral, deterministic, and independent of implementation details.
- Verify scenarios through the supported component API.
- Keep each persisted scenario traceable to its executable test.
- Do not use Gherkin for ordinary implementation properties.

## C++ design

Use idiomatic C++23.

Use the type system to make invalid states unrepresentable where practical.

Prefer ranges and standard algorithms where they improve clarity.

Use ports and adapters where external dependencies would otherwise prevent deterministic testing of core behavior.

Prefer small modules with explicit ownership and dependencies.

## C++ conventions

### Naming and formatting

Use Google-style C++ naming:

- `PascalCase` for types and functions.
- `snake_case` for variables and parameters.
- `snake_case_` for class data members.
- `kPascalCase` for constants.
- `snake_case` for filenames and namespaces.

Follow existing local conventions where they are more specific.

Follow repository rules enforced by `.clang-format`, `.clang-tidy`, the compiler, and Bazel. Prefer mechanical enforcement over written guidance where practical.

### Header conventions

- `internal/` identifies code not exposed outside its owning component.
- `impl/` contains implementation details behind a component or subcomponent API.
- `detail/` contains template or header implementation machinery that may be required by API headers but is not supported API.
- Never expose `internal/` implementation through a repository-public header.
- Never expose `impl/` headers through an API header.
- Consumers must not depend directly on `detail/`.

## Change discipline

Work in small, coherent increments.

After each increment:

- keep the build and tests green;
- refactor before adding further complexity;
- preserve or improve component boundaries;
- avoid introducing broader dependencies than required.

When existing code conflicts with these principles, improve it locally where safe rather than propagating the existing coupling.

# Repository Guidelines

## Project Structure & Modules

This is a small C++23 terminal editor. The application entry point and terminal
wiring are in `editor.cpp`. Editing commands, navigation, viewport state, and
frame snapshots live behind `//editor_core:api` in
`editor_core/editor.{h,cpp}`. Input-protocol decoding lives behind
`//key_decoder:api` in `key_decoder/decoder.{h,cpp}`; `read_key.cpp` adapts stdin
to its byte-source interface. Both packages contain API tests and acceptance
scenarios. `terminal::RawMode` in `raw_mode.{h,cpp}` owns scoped terminal settings
behind `//:raw_mode`; its tests use isolated PTYs. `terminal::InputProtocol` in
`input_protocol.{h,cpp}` requests scoped keyboard enhancement without probing
stdin, behind `//:input_protocol`. Checked descriptor I/O lives
behind `//terminal_io:api`; `//:read_key` exposes key/NoKey/EOF results, and
`//:terminal_output` exposes buffered rendering with checked writes and scoped
SIGPIPE handling. The document model and its byte, logical-line, and wrapped-row
views live in `document/` behind `//document:api`; its API tests and acceptance
scenarios are kept in that package. `//document:test_api` exposes the same API
with the debug throwing-contract configuration for tests. Storage primitives
and their tests live in
`document/internal/gap_buffer/` and `document/internal/line_starts/`, each behind
its own `:api` target with package-private implementation targets. The document
package is the only external consumer of both storage APIs; the root file
loader depends directly on `//document/types:api` and cannot access storage.
Each storage package exposes `:test_api` with the shared debug throwing-contract
configuration from `//:contract_test_mode`. Unsupported concrete storage
declarations live in `document/detail/` so the document keeps inline ownership
without including internal storage API headers. Private layout targets serve
only the document and their owning storage implementations. Logical-line
ordinals are owned by `document/line_number.h`. Invariant diagnostic types live
behind `//contracts:api`; checks remain in `contracts/impl/checks.h` behind a
restricted implementation target. Shared byte types and size-limit helpers
live behind `//document/types:api` and `//:size_limits`, respectively. Allocation
failure tests live with document and each storage package, using their supported
`:test_api` targets. Shared injection and failure discovery live behind the
narrowly visible, test-only `//test_support:allocation_failure` target; isolated
failure test binaries link statically and always link its global allocation
overrides. Tests are kept beside the code as `*_tests.cpp`.
Bazel dependencies are declared in `MODULE.bazel`.

## Build, Test, and Run

Install Bazelisk, then run commands from the repository root. Bazelisk reads
the pinned Bazel version from `.bazelversion`; dependencies are declared in
`MODULE.bazel` and locked in `MODULE.bazel.lock`.

```sh
bazel build //:editor
bazel test //...
bazel test -c dbg //...
bazel run //:editor -- [file]
python3 tools/verify.py
```

Run storage tests with
`bazel test //document/internal/gap_buffer:gap_buffer_tests` and
`bazel test //document/internal/line_starts:line_starts_tests` (or another
`*_tests` target). Each storage package also has `:edit_failure_tests`.
Run document allocation-failure scenarios with
`bazel test //document:edit_failure_tests`. Run document API tests with
`bazel test //document:document_tests` and view tests with
`bazel test //document:all`. Run editor API tests with
`bazel test //editor_core:editor_tests`, and decoder API tests with
`bazel test //key_decoder:decoder_tests`. Run terminal lifetime tests with
`bazel test //:raw_mode_tests`, and protocol-session tests with
`bazel test //:input_protocol_tests`. Run input/output adapter tests with
`bazel test //:read_key_tests //:terminal_tests`, and focused syscall implementation
tests with `bazel test //terminal_io:syscall_tests`. The project uses C++23, GoogleTest,
and Microsoft GSL; the editor relies on POSIX terminal APIs.

## Repository Verification

Run `python3 tools/verify.py` for formatting, persisted acceptance traceability,
and the checker's temporary-fixture tests. It accepts
`--clang-format /path/to/clang-format`. The scripts resolve repository paths from
their own locations and can run from any directory using absolute script paths.
They use Python's standard library and introduce no runner dependency.

Run `python3 tools/acceptance/check_traceability.py` for traceability alone, or
`python3 tools/acceptance/check_traceability_tests.py` for its focused tests.
The checker discovers active tracked and untracked Git files, excludes ignored
output, and does not follow symlinks. It requires at least one immediately
adjacent `TEST`, `TEST_F`, or `TEST_P` binding per persisted scenario; multiple
bindings are allowed. Unsupported Gherkin constructs, duplicate scenario
identities, malformed annotations, and missing references fail with path/line
diagnostics. Supported syntax and limits are documented in
[tools/acceptance/README.md](tools/acceptance/README.md). Continue running the
Bazel build and default/debug test suites separately.

## Coding Style & File Conventions

Run `python3 tools/check_format.py` with clang-format 21.1.8 to check all tracked
and untracked C++ sources against `.clang-format`. The check excludes ignored
build output. Use `--clang-format /path/to/clang-format` if
the pinned version is installed under a different executable name. The check
reports formatting errors without modifying files.

Follow `.clang-format` (Google-derived C++ style, two-space indentation, 80-column limit, left-aligned pointers). Keep declarations in `.h` files and implementations in `.cpp` files. Use the naming conventions in Engineering Guidance above; test files use the matching module name plus `_tests.cpp`. The build treats `-Wall -Wextra -Wpedantic` warnings as errors, so keep changes warning-clean.

## Testing Guidelines

Add or update a `*_tests.cpp` suite for behavior changes, keeping tests focused
on the corresponding module. Run `bazel test //...`; no coverage threshold is
configured. Test targets relax only the
`sign-compare` error for GCC 15 diagnostics emitted by GoogleTest assertion
templates.

Bind persisted acceptance scenarios with GoogleTest through the owning
component's supported API. Place these annotations immediately above each
binding, including parameterized tests:

```cpp
// Feature: <repo-relative-path-to-feature-file>
// Scenario: <exact scenario title>
TEST(...) {
  ...
}
```

Allocation-failure tests should discover allocation points without asserting
their number or order, and verify unchanged observable state and subsequent
successful editing.

## Commits & Pull Requests

Git history is available through `git log`. Use a short imperative subject (for example, `Add wrapped row range tests`). Pull requests should explain the behavior change, note relevant test results, and link an issue when applicable. Include terminal screenshots only for visible UI changes.

## Configuration Notes

Keep generated build files out of source changes.
