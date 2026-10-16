# Unplugged repository guide

Unplugged is a C++23 terminal editor. This guide records repository-specific
structure and commands; behavior contracts belong in the owning component's
specification.

## Where code lives

- `applications/editor/` composes components, runs the main loop, and renders
  frames. `//:editor` is the compatibility build/run alias for
  `//applications/editor:editor`. Components do not depend on the application.
- `document/` owns document bytes, edits, logical lines, and wrapped-row views.
  Its contract is [document.md](../document/docs/specs/document.md).
- `editor_core/` owns editing commands, navigation, viewport state, and frame
  snapshots. Its contract is [editor.md](../editor_core/docs/specs/editor.md).
- `key/` owns shared key value types; `key_decoder/` owns protocol decoding.
- `key_decoder/ports/byte_source/` owns the byte-source port and conformance
  checks. The terminal's FD adapter implements that contract; see
  [byte_source.md](../key_decoder/ports/byte_source/docs/specs/byte_source.md).
- `terminal/` owns POSIX terminal lifetimes, input protocol, key acquisition,
  and buffered output. Its contract is
  [terminal.md](../terminal/docs/specs/terminal.md).
- `terminal_io/` owns checked descriptor I/O; `file_loader/` owns file loading.
- `contracts/` owns invariant diagnostic types. Storage primitives under
  `document/internal/` are used by the document component.

Repository-visible component tests and persisted features live in each
component's `tests/` and `features/` directories. Internal subcomponent tests
live in their owning `tests/` directories. Test sources use `*_tests.cpp`.
Implementation `.cpp` files live at their component or subcomponent root.
Repository-visible APIs use `include/<component>/`; internal subcomponent and
port APIs remain at their package roots. Do not add `src/` directories or empty
source/test directories to header-only packages. Bazel dependencies are
declared in `MODULE.bazel`.
Read the owning specification and public headers for supported behavior and API
boundaries; this map is not a substitute for those contracts.

## Tests and contributions

Add or update a focused test suite for behavior changes. Boundary tests should
use the supported component API. Allocation-failure tests discover allocation
points without asserting their number or order, check that observable state is
unchanged after failure, and verify that a later edit succeeds. Test targets
relax `sign-compare` warnings emitted by GoogleTest assertion templates for the
GCC 15 diagnostics; do not broaden that exception without need.

Use a short imperative commit subject. Pull requests should describe the
behavior change, report relevant checks, and link an issue when applicable.
Include terminal screenshots for visible UI changes. Keep generated build
files out of source changes.

## Build and run

Install Bazelisk and a compiler with C++23 support. Bazelisk reads the pinned
version from `.bazelversion`; dependencies are declared in `MODULE.bazel` and
locked in `MODULE.bazel.lock`.

```sh
bazel build //:editor
bazel run //:editor -- [file]
```

The editor opens an existing file and edits it in memory; it does not save
changes. Navigation and wrapping count bytes, not Unicode display cells.

## Verification

Run these commands from the repository root:

```sh
python3 tools/verify.py
bazel build //:editor
bazel test //...
bazel test -c dbg //...
```

`tools/verify.py` preserves the repository entry point and runs the C++ format
check, acceptance traceability check and fixture tests, the architecture checker,
and the complete focused architecture test suite. It continues all checks after
a failure and returns failure if any check fails. The format check requires
clang-format 21.1.8 and accepts `--clang-format /path/to/clang-format`, including
when passed to `tools/verify.py`. It checks active tracked and untracked C++
sources against `.clang-format`, excluding ignored build output. The traceability
checker validates supported feature syntax and requires at least one adjacent
`TEST`, `TEST_F`, or `TEST_P` binding per scenario. Its guide explains the syntax
and limits: [acceptance traceability](../tools/acceptance/README.md).

The [architecture checker](../tools/architecture/README.md) uses Git and the Bazel
version pinned by `.bazelversion` (normally supplied by Bazelisk) to run
loading-phase queries. These queries and their integration fixtures need normal
cache/download access but no C++ compiler or SDK. Definite architecture violations
and Git/Bazel operational errors fail verification; nonblocking
not-checked findings do not. Both the checker and `tools/verify.py` derive the
repository root from their script locations, so they can be invoked by absolute
path from another directory. Tests for verification integration run directly with
`python3 tools/architecture/verify_tests.py` and are included in the full
architecture discovery suite.

CI runs the same verification entry point, editor build, and default and debug
Bazel test suites in [.github/workflows/ci.yml](../.github/workflows/ci.yml).
These remain distinct checks: `tools/verify.py` runs architecture loading queries
and fixture tests, but does not run Bazel builds or product test targets.
