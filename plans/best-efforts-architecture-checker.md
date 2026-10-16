# A simple public-header include check

Status: current narrow check implemented; portable boundary-check expansion is
proposed and has not started.

The goal is to catch obvious accidental boundary breakage during ordinary
verification. Keep the check small and source based. Do not build a full
component graph, interpret Bazel declarations, or try to prove architectural
correctness.

`tools/check_architecture.py` uses Git to list active tracked and untracked
headers beneath conventional `include/` directories. It reads include-shaped
lines and reports a line if the path contains an `impl/` segment or a `..`
segment. This simple scan can also match text in block comments and inactive
preprocessor branches; review the reported line if one appears. The check uses
only the Python standard library; it needs neither Bazel nor a C++ compiler.
`tools/verify.py` runs it with formatting and acceptance traceability.

The current scan is deliberately shallow. It does not follow transitive includes,
resolve preprocessor conditions, infer component ownership, or check dependency
cycles. The proposed follow-up uses only conventional paths and the presence of
BUILD files to compare ownership; it does not interpret BUILD declarations or
construct a dependency graph. Keep existing compiler based API tests and Bazel
visibility checks as the stronger checks for their respective properties.

## Follow-up: portable direct-include boundary rules

Keep the checker usable on Windows, Linux, and macOS with Python's standard
library and Git only. Do not require Bazel, a compiler, clang, or a C++ parser.
The checker reports direct source-level include violations; it does not claim to
prove that C++ declarations preserve an API boundary.

### Rules to enforce

- Public headers under `include/` must not directly include an `impl/` header or
  use a path with `.` or `..` traversal segments. Preserve the existing rule and
  normalize both slash styles before checking segments.
- Public headers must not directly include test headers or headers beneath a
  `tests/` directory.
- A public header may include an `internal/` API header only from the same
  top-level component. This preserves the existing `document` composition of
  its private gap-buffer and line-start members while rejecting cross-component
  internal includes.
- A `detail/` header may be included directly only from a file in the same
  Bazel package as that detail header. This allows an API header or implementation
  in the owning package to use support declarations, while rejecting direct
  detail dependencies from client packages.
- Do not attempt to detect internal or detail types in public/protected C++
  declarations. That requires language parsing and remains outside this portable
  check.

### Implementation steps

- [ ] Extend discovery to retain normalized repository-relative paths for active
  headers and identify Bazel package roots from `BUILD` / `BUILD.bazel` files.
- [ ] Build an index of repository headers by repository path and by public
  include spelling (remove the `include/` directory where present). If a
  private include spelling maps ambiguously, report it as ambiguous rather than
  guessing its owner. Ignore unresolved external/system includes.
- [ ] Parse only literal `#include` directives. Normalize `\\` to `/` for
  path-rule evaluation. Keep macro-based include operands outside the check and
  document that limitation.
- [ ] Apply the `impl`, traversal, tests, same-component `internal`, and
  same-package `detail` rules above. Determine a header's component from its
  repository location and its package from the nearest ancestor containing a
  Bazel BUILD file; do not parse BUILD language contents.
- [ ] Add isolated Python fixture tests for accepted and rejected includes,
  including Windows-style separators, nested Bazel packages, the documented
  `document/internal/` exception, external headers, ambiguous paths, comments,
  inactive preprocessor branches, and symlinks.
- [ ] Preserve diagnostics with repository-relative path, line number, rule,
  and include spelling. Operational discovery/read failures must remain distinct
  from policy violations.
- [ ] Update `guidance/repository.md` and this plan to state exactly which rules
  are enforced and which C++ API properties remain unverified.
- [ ] Add the fixture suite to `tools/verify.py`; run it and the verification
  entry point on at least Windows, Linux, and macOS CI runners if those runners
  are available.

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
