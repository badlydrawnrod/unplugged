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
SIGPIPE handling. Document and editing primitives live in root-level pairs such as
`document.{h,cpp}`, `gap_buffer.{h,cpp}`, and `line_starts.{h,cpp}`. Tests are kept
beside the code as `*_tests.cpp`. `old_20260924/` contains archived experiments;
do not add new production code there. Bazel dependencies are declared in
`MODULE.bazel`.

## Build, Test, and Run

Install Bazelisk, then run commands from the repository root. Bazelisk reads
the pinned Bazel version from `.bazelversion`; dependencies are declared in
`MODULE.bazel` and locked in `MODULE.bazel.lock`.

```sh
bazel build //:editor
bazel test //...
bazel run //:editor -- [file]
```

Run one test target with `bazel test //:gap_buffer_tests` (or another
`*_tests` target). Run editor API tests with
`bazel test //editor_core:editor_tests`, and decoder API tests with
`bazel test //key_decoder:decoder_tests`. Run terminal lifetime tests with
`bazel test //:raw_mode_tests`, and protocol-session tests with
`bazel test //:input_protocol_tests`. Run input/output adapter tests with
`bazel test //:read_key_tests //:terminal_tests`, and focused syscall implementation
tests with `bazel test //terminal_io:syscall_tests`. The project uses C++23, GoogleTest,
and Microsoft GSL; the editor relies on POSIX terminal APIs.

## Coding Style & File Conventions

Follow `.clang-format` (Google-derived C++ style, two-space indentation, 80-column limit, left-aligned pointers). Keep declarations in `.h` files and implementations in `.cpp` files. Use the naming conventions in Engineering Guidance above; test files use the matching module name plus `_tests.cpp`. The build treats `-Wall -Wextra -Wpedantic` warnings as errors, so keep changes warning-clean.

## Testing Guidelines

Add or update a `*_tests.cpp` suite for behavior changes, keeping tests focused
on the corresponding module. Run `bazel test //...`; no coverage threshold is
configured. Test targets relax only the
`sign-compare` error for GCC 15 diagnostics emitted by GoogleTest assertion
templates.

## Commits & Pull Requests

Git history is available through `git log`. Use a short imperative subject (for example, `Add wrapped row range tests`). Pull requests should explain the behavior change, note relevant test results, and link an issue when applicable. Include terminal screenshots only for visible UI changes.

## Configuration Notes

Keep generated build files out of source changes.
