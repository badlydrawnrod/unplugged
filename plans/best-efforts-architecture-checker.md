# A simple public-header include check

Status: implemented.

The goal is to catch obvious accidental boundary breakage during ordinary
verification. Keep the check small and source based. Do not build a component
graph, interpret Bazel declarations, or try to prove architectural correctness.

`tools/check_architecture.py` uses Git to list active tracked and untracked
headers beneath conventional `include/` directories. It reads include-shaped
lines and reports a line if the path contains an `impl/` segment or a `..`
segment. This simple scan can also match text in block comments and inactive
preprocessor branches; review the reported line if one appears. The check uses
only the Python standard library; it needs neither Bazel nor a C++ compiler.
`tools/verify.py` runs it with formatting and acceptance traceability.

The scan is deliberately shallow. It does not follow transitive includes, resolve
preprocessor conditions, infer component ownership, inspect BUILD files, or check
dependency cycles. Those checks need more parsing and can overlap with ordinary
Bazel enforcement. Add a narrow rule only when a concrete accidental breakage
shows a simple pattern would catch it. Keep existing compiler based API tests and
Bazel visibility checks as the stronger checks for their respective properties.

## Verification

From any working directory, run:

```sh
python3 /path/to/unplugged/tools/check_architecture.py
python3 /path/to/unplugged/tools/verify.py
```

The checker exits 0 when it finds no suspicious direct include, 1 when it finds
one or more, and 2 when Git or a required header cannot be read. Bazel build and
default/debug test commands remain separate.
