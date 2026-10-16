# A simple public-header include check

Status: direct-include rules, fixture suite, and CI matrix implemented; hosted
Windows and macOS execution remains to be confirmed.

The goal is to catch obvious accidental boundary breakage during ordinary
verification. Keep the check small and source based. Do not build a full
component graph, interpret Bazel declarations, or try to prove architectural
correctness.

`tools/check_architecture.py` uses Git to list active tracked and untracked
headers beneath conventional `include/` directories. It inspects literal include
directives, normalizes forward and backslash separators, and reports direct
`impl/`, `.` and `..` path segments. For uniquely resolved repository headers,
it rejects test headers, cross-component `internal/` includes, and `detail/`
includes from a different Bazel package. If a private spelling matches multiple
repository headers, it reports the ambiguity instead of choosing one. Unresolved
system and external headers are ignored for ownership checks; lexical `impl/`
and path traversal checks still apply. Macro-based includes are ignored.

This simple scan can also match text in block comments and inactive preprocessor
branches; review the reported line if one appears. The check uses only the Python
standard library and Git; it needs neither Bazel nor a C++ compiler. It does not
follow transitive includes, resolve preprocessor conditions, inspect declarations
for internal or detail types in public signatures, interpret BUILD declarations,
or check dependency cycles. Keep existing compiler based API tests and Bazel
visibility checks as the stronger checks for their respective properties.
The checker scans public headers only; direct `detail/` includes from `.cpp` and
test files are outside its scope.

## Follow-up: portable direct-include boundary rules

Keep the checker usable on Windows, Linux, and macOS with Python's standard
library and Git only. Do not require Bazel, a compiler, clang, or a C++ parser.
The checker reports direct source-level include violations; it does not claim to
prove that C++ declarations preserve an API boundary.

### Rules to enforce

- Public headers under `include/` must not directly include an `impl/` header or
  use a path with `.` or `..` traversal segments. Preserve the existing rule and
  normalize both slash styles before checking segments.
- Public headers must not directly include test headers (under `tests/` or named
  `*_tests`).
- A public header may include an `internal/` API header only from the same
  top-level component. This preserves the existing `document` composition of
  its private gap-buffer and line-start members while rejecting cross-component
  internal includes.
- A scanned public header may include a `detail/` header only from the same
  Bazel package. This allows API headers to use support declarations while
  rejecting direct detail includes from public headers in client packages.
- Do not attempt to detect internal or detail types in public/protected C++
  declarations. That requires language parsing and remains outside this portable
  check.

### Implementation steps

- [x] Extend discovery to retain normalized repository-relative paths for active
  headers and identify Bazel package roots from `BUILD` / `BUILD.bazel` files.
- [x] Build an index of repository headers by repository path and by public
  include spelling (remove the `include/` directory where present). If a
  private include spelling maps ambiguously, report it as ambiguous rather than
  guessing its owner. Ignore unresolved external/system includes for ownership
  checks; retain the direct lexical `impl/` and traversal checks.
- [x] Parse only literal `#include` directives. Normalize forward and backslash
  separators for path-rule evaluation. Keep macro-based include operands outside
  the check and document that limitation.
- [x] Apply the `impl`, traversal, tests, same-component `internal`, and
  same-package `detail` rules above. Determine a header's component from its
  repository location and its package from the nearest ancestor containing a
  Bazel BUILD file; do not parse BUILD language contents.
- [x] Add isolated Python fixture tests for accepted and rejected includes,
  including Windows-style separators, nested Bazel packages, the documented
  `document/internal/` exception, external headers, ambiguous paths, comments,
  inactive preprocessor branches, and symlinks.
- [x] Preserve diagnostics with repository-relative path, line number, rule,
  and include spelling. Operational discovery/read failures must remain distinct
  from policy violations.
- [x] Update `guidance/repository.md` and this plan to state exactly which rules
  are enforced and which C++ API properties remain unverified.
- [x] Add the fixture suite to `tools/verify.py` and configure CI to run the
  verification entry point on Ubuntu, Windows, and macOS.
- [ ] Confirm the cross-platform verification jobs pass after this change is
  pushed and CI runs on all three operating systems.

### Completion criteria

- The checker runs without C++ tooling or Bazel on Windows, Linux, and macOS.
- Each rule has fixtures for both an accepted case and a violation, including
  the repository's documented same-component `internal/` use.
- The checker never infers type exposure or transitive dependency behavior.
- CI and the repository guide describe the implemented scope accurately.

## Verification

From any working directory, run:

```sh
python3 /path/to/unplugged/tools/check_architecture.py
python3 /path/to/unplugged/tools/verify.py
```

The checker exits 0 when it finds no suspicious direct include, 1 when it finds
one or more, and 2 when Git or a required header cannot be read. Bazel build and
default/debug test commands remain separate.
