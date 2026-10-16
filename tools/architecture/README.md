# Architecture checker

This tooling owns the contract for a small, deterministic architecture checker.
The foundation is implemented: Git discovery and structured Bazel fact loading,
with fixtures. Rule evaluation, the `check.py` command, and integration with
`tools/verify.py` are subsequent implementation steps. No architecture verdict is
produced yet. Existing compiler-based public-header assertions remain in place.

## Operating contract

The completed checker will detect a few observable mistakes, rather than prove
architectural correctness. The same source files, evaluated build facts, tool
versions, and options must produce the same findings and exit status.

- A definite violation of a supported rule fails the check.
- Unsupported or ambiguous arrangements produce nonblocking **not checked**
  findings identifying the location, affected rule, and reason. Unaffected checks
  continue. Potential violations requiring unresolved conditional choices belong
  in this category.
- Git/Bazel failures and unreadable required inputs are operational errors and
  fail the command. A failed query must never become a successful empty audit.
- Text output will separate definite violations from coverage limitations, sort
  findings by repository-relative path, line, and rule, and give a short
  checked/not-checked summary. Exit 0 will mean no supported-rule violation was
  found. Machine-dependent timings do not belong in findings.
- Facts come from source, layout, and evaluated Bazel declarations. There is no
  component registry, exception inventory, or checked-in generated graph.
- Inspection requires no C++ compiler or platform SDK. Ordinary Bazel builds
  continue to enforce effective visibility and `testonly`.

## Planned rules

| Rule | Supported check |
| --- | --- |
| Includes | Follow resolvable literal first-party includes; reject a supported API header whose include closure reaches `impl/`, reporting the path. |
| Private access | Reject clear direct client includes of another component's `internal/` or `detail/` headers, preserving parent/private composition and explicitly selected test access. |
| Entry targets | Reject clear direct production API bypasses in evaluated code dependencies, respecting narrow implementation grants and test helpers; uncertain edges are not checked. |
| Cycles | Report cycles composed entirely of resolved unconditional code dependencies between owning components. Discard edges within one owner; retain edges between parent and nested components. |

A package with a supported C++ `:api` target defines a component. Descendant
packages inherit the nearest component unless they define another supported API.
A nested component under `internal/` or `detail/` is still a separate component.
Header layout supplies evidence where unambiguous; conflicting evidence will
produce coverage limitations. No ownership inference is implemented in this step.

## Implemented foundation

`discovery.discover_files(root)` uses Git's NUL-delimited cached and untracked
inventory, excluding ignored untracked output. It returns immutable, sorted,
unique repository-relative POSIX paths for C/C++ inputs and active BUILD files,
and their package paths (the empty string denotes the root package). Supported
suffixes are `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, and `.inc`.
Deleted paths, nonregular files, symlink files, and paths with symlink ancestors
are excluded without following linked directories. Tracked files remain visible
even if newly ignored. Both BUILD names are retained when present; each package
is queried once and Bazel chooses `BUILD.bazel` over `BUILD`.

`query.load_facts(root, inventory)` reads `.bazelversion`, asks Bazelisk for that
version using `USE_BAZEL_VERSION`, and verifies the executable's version. It runs
loading-phase `bazel query` over an explicit `set("//package:*" ...)`, with
`--output=streamed_jsonproto --proto:flatten_selects=false`. It does not use a
recursive target pattern or analyze a configuration. Manual and incompatible
targets remain visible. Bazel still loads declarations and their ordinary loads;
this is not an isolated parser for BUILD files.

The small private records in `facts.py` retain:

- Target labels, rule kinds, and repository-relative declaration locations.
- `srcs`, `hdrs`, and `textual_hdrs` membership, as labels. Filegroups, generated
  inputs, aliases, and external labels remain distinguishable from local files;
  the loader does not resolve indirect membership.
- Separate `deps` and `implementation_deps`. Data, tools, implicit toolchains,
  and merged `ruleInput` lists are excluded from code dependencies.
- `testonly`, evaluated visibility strings, and whether attributes were
  explicitly specified. Package-default visibility is retained too. These are
  evidence for later access checks, not proof of effective permission.
- `include_prefix`, `strip_include_prefix`, `includes`, `local_includes`, and
  alias `actual` values, without yet resolving includes or aliases.
- Source-file and generated-file labels and locations, with generating-rule
  labels for generated files. Generated headers need not exist on disk.

Ordinary values and ordered concatenated selector segments are separate. Each
selector retains its condition labels, branch values, default presence, and
no-match message. Empty branch values remain empty; branches using an attribute
default retain that marker. Nothing evaluates or merges mutually exclusive
choices. Bazel encodes a constant segment in a concatenation as a selector with
only `//conditions:default`; the reader retains that representation. Condition
labels themselves are not code dependencies.

The reader supports string/label scalars, string/label lists, booleans, and
selectors over those types. Unknown types in inspected attributes retain a
`limitation` on the attribute for later not-checked reporting. Other attributes
are intentionally outside the extracted facts. Unknown query record kinds are
retained as repository-level limitations alongside the usable facts. Non-C++
rules retain their kinds without being assigned an architectural role.
Malformed structured tool output,
nonzero tool exits (even with partial stdout), version mismatches, missing pins,
and successful empty query output for active packages raise `OperationalError`.

## Limits and remaining work

The foundation assumes a repository root and its readable numeric Bazel pin. It
does not inspect arbitrary source suffixes or symlinked inputs. It provides no
C++ scanner, header resolver, dependency policy, cycle detector, findings report,
or public checker command yet.

Later rules will remain conservative: computed includes, ambiguous mappings,
unavailable generated headers, indirect inputs outside the supported subset,
and unresolved conditional combinations will be coverage limitations. Standard
and external headers stay outside first-party include traversal. Explicit
package visibility grants can supply access evidence; package-group and symbolic
macro visibility semantics will remain Bazel's responsibility. Exact supported
grant forms must be documented with rule fixtures when those rules are added.

## Verification

From the repository root, run all foundation fixtures:

```sh
python3 -m unittest discover -s tools/architecture -p '*_tests.py'
```

Or run each suite separately:

```sh
python3 tools/architecture/discovery_tests.py
python3 tools/architecture/query_tests.py
python3 tools/architecture/query_integration_tests.py
```

Discovery fixtures create real temporary Git repositories. The query unit tests
exercise retained facts and operational failures. The integration fixture runs
the pinned Bazel in a temporary repository with a small Starlark rule, so it
needs Bazelisk/Bazel but no C++ toolchain. It checks conditional dependencies and
headers, inherited and selected visibility, test context, manual/incompatible
targets, generated inputs, repeatability, package names containing spaces,
ignored package exclusion, and an actual BUILD loading failure. It uses batch
mode and a temporary Bazel output root. Bazelisk may need its normal version
download/cache access.

Keep the existing repository checks separate:

```sh
python3 tools/verify.py
bazel build //:editor
bazel test //...
bazel test -c dbg //...
```

`tools/verify.py` does not run the new fixtures yet; that integration belongs to
the final implementation step.
