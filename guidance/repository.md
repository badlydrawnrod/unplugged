# Unplugged repository guide

Unplugged is a C++23 terminal editor. This guide records repository-specific
structure and commands; behavior contracts belong in the owning component's
README.md.

## Where code lives

- `applications/editor/` composes components, runs the main loop, and renders
  frames. `//:editor` is the compatibility build/run alias for
  `//applications/editor:editor`. Components do not depend on the application.
- `document/` owns document bytes, edits, logical lines, and wrapped-row views.
  Its contract is [README.md](../document/README.md).
- `editor_core/` owns editing commands, navigation, viewport state, and frame
  snapshots. Its contract is [README.md](../editor_core/README.md).
- `key/` owns shared key value types; `key_decoder/` owns protocol decoding.
- `key_decoder/ports/byte_source/` owns the byte-source port and conformance
  checks. The terminal's FD adapter implements that contract; see
  [README.md](../key_decoder/ports/byte_source/README.md).
- `terminal/` owns POSIX terminal lifetimes, input protocol, key acquisition,
  and buffered output. Its contract is
  [README.md](../terminal/README.md).
- `terminal_io/` owns checked descriptor I/O; `file_loader/` owns file loading.
- `contracts/` owns invariant diagnostic types. Storage primitives under
  `document/internal/` are used by the document component.

Each component's `README.md` is its current behavior contract. Keep
repository-visible APIs in `include/<component>/`, significant persisted
behavior examples in `features/`, executable checks in `tests/`, and unfinished
component work in `plans/`. Apply the same README and neighboring-directory
convention to substantial nested ports or subcomponents. Use root `plans/` for
cross-cutting proposals. Create these directories only when needed.

Internal subcomponent tests live in their owning `tests/` directories. Test
sources use `*_tests.cpp`. Implementation `.cpp` files live at their component
or subcomponent root. Internal subcomponent and port APIs remain at their
package roots. Do not add `src/` directories or empty source/test directories
to header-only packages. Bazel dependencies are declared in `MODULE.bazel`.
Read the owning README and public headers for supported behavior and API
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

Run these commands from the repository root (`python` can replace `python3` on
Windows):

```sh
python3 tools/verify.py
bazel build //:editor
bazel test //...
bazel test -c dbg //...
```

`tools/verify.py` preserves the repository entry point and runs the C++ format
check, acceptance traceability check and fixture tests, and a small source-pattern
check for suspicious includes in public headers. It continues all checks after a
failure and returns failure if any check fails. The format check requires
clang-format 21.1.8 and accepts `--clang-format /path/to/clang-format`, including
when passed to `tools/verify.py`. It checks active tracked and untracked C++
sources against `.clang-format`, excluding ignored build output. The traceability
checker validates supported feature syntax and requires at least one adjacent
`TEST`, `TEST_F`, or `TEST_P` binding per scenario. Its guide explains the syntax
and limits: [acceptance traceability](../tools/acceptance/README.md).

The [architecture check](../tools/check_architecture.py) scans active tracked and
untracked headers beneath `include/` directories. It inspects literal include
directives, normalizes forward and backslash separators, and reports `impl/`,
`.` and `..` path segments. For uniquely resolved repository headers, it also
rejects test headers (under `tests/` or named `*_tests`), cross-component
`internal/` includes, and `detail/` includes from a different Bazel package. If
a private spelling matches multiple repository headers, it reports the
ambiguity instead of choosing one. Unresolved system and external headers are
ignored for ownership checks; the lexical `impl/` and path traversal checks
still apply. Macro-based includes are ignored.

The checker uses Git and the Python standard library; it requires neither Bazel
nor a C++ compiler. It is a direct-include scan of public headers: block comments
and inactive preprocessor branches can produce findings, and direct `detail/`
includes from `.cpp` or test files are outside its scope. It does not inspect C++
declarations for internal or detail types in public signatures, follow transitive
includes, or interpret Bazel visibility declarations. Bazel builds remain
responsible for target dependency enforcement. The checker, `tools/verify.py`,
and acceptance tooling derive the repository root from their script locations.

CI runs the same verification entry point, editor build, and default and debug
Bazel test suites in [.github/workflows/ci.yml](../.github/workflows/ci.yml).
These remain distinct checks: `tools/verify.py` runs formatting, acceptance
traceability and its fixture suite, and the architecture scan and its fixture
suite. CI runs this verification entry point on Ubuntu, Windows, and macOS; the
Bazel build and product test targets run on Ubuntu. The verification entry point
does not run Bazel builds or product test targets.
