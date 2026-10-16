# Architecture checker

This tooling owns the contract for a small, deterministic architecture checker.
Git discovery, structured Bazel fact loading, and the four supported rules are
implemented, with a public `check.py` command and focused fixtures covering the
step 3 completion criteria. Integration with `tools/verify.py` remains in the plan.
Existing compiler-based public-header assertions remain in place.

Run from the repository root:

```sh
python3 tools/architecture/check.py
```

The default repository is relative to the script, so invocation from another
directory works too. Use `--repo /path/to/checkout` to inspect another repository.
Exit status is 0 for no supported violation, 1 for definite violations, and 2 for
operational errors. Coverage limitations alone do not fail the command.

## Operating contract

The checker detects a few observable mistakes, rather than proving
architectural correctness. The same source files, evaluated build facts, tool
versions, and options must produce the same findings and exit status.

- A definite violation of a supported rule fails the check.
- Unsupported or ambiguous arrangements produce nonblocking **not checked**
  findings identifying the location, affected rule, and reason. Unaffected checks
  continue. Potential violations requiring unresolved conditional choices belong
  in this category.
- Git/Bazel failures and unreadable required inputs are operational errors and
  fail the command. A failed query must never become a successful empty audit.
- Text output separates definite violations from coverage limitations, sorts
  findings by repository-relative path, line, and rule, and gives a short
  checked/not-checked summary. Exit 0 means no supported-rule violation was
  found. Machine-dependent timings do not belong in findings.
- Facts come from source, layout, and evaluated Bazel declarations. There is no
  component registry, exception inventory, or checked-in generated graph.
- Inspection requires no C++ compiler or platform SDK. Ordinary Bazel builds
  continue to enforce effective visibility and `testonly`.

## Rules

| Rule | Supported check |
| --- | --- |
| Includes | Follow resolvable literal first-party includes; reject a supported API header whose include closure reaches `impl/`, reporting the path. |
| Private access | Reject clear direct client includes of another component's `internal/` or `detail/` headers, preserving parent/private composition and explicitly selected test access. |
| Entry targets | Reject clear direct production API bypasses in evaluated code dependencies, respecting narrow implementation grants and test helpers; uncertain edges are not checked. |
| Cycles | Report cycles composed entirely of resolved unconditional code dependencies between owning components. Discard edges within one owner; retain edges between parent and nested components. |

A package with a supported C++ `:api` target defines a component. Descendant
packages inherit the nearest component unless they define another supported API.
A nested component under `internal/` or `detail/` is still a separate component.
Targets supported as C++ code are `cc_library`, `cc_binary`, and `cc_test`.
Only `cc_library` establishes a supported `:api` boundary. Unsupported `:api`
kinds make affected ownership uncertain; a supported nested boundary still owns
its descendants. An API exporting a header owned by a different component gives
conflicting evidence, so ownership for that header remains uncertain.

Same-package library variants count as entry targets when their nonempty `hdrs`,
`textual_hdrs`, `include_prefix`, and `strip_include_prefix` match the canonical
API without unresolved choices. Names such as `test_api` alone grant no access.

## Supported rule behavior

The include scanner masks comments, ordinary strings, character literals, and
raw strings, and handles backslash-newline splicing while retaining physical
line numbers. It reads literal `#include "..."` and `#include <...>` directives.
Conventional whole-file `#ifndef`/`#define` guards are treated as guards;
other preprocessor branches are unresolved, including `#if 0` and `#if 1`.
Conditional source/header membership and include mappings also make affected
paths potential findings. The preprocessor is never evaluated.

Existing quoted relative paths take precedence. Otherwise, repository-relative
paths and unique simple header aliases are resolved against active inputs.
Supported aliases use normalized relative `include_prefix`, package-relative or
repository-absolute `strip_include_prefix`, and repository-contained
`includes`/`local_includes` directories. Empty strip prefixes are unset;
`include_prefix` alone implicitly strips the package path, as in the pinned
`rules_cc`. Prefix/strip combinations needing unresolved choices are conditional.
Alias lookup is global and syntactic; it does not reproduce a compiler's
per-translation-unit search paths. Ambiguous aliases are not checked.

API headers, including supported variants, are traversed through resolved
first-party includes. A path reaching an `impl/` directory reports the full
offending include chain. An API header itself in `impl/` also fails. Access grants
never exempt this rule. Each closure carries conditional evidence along its path.

Direct private-header includes inspect paths containing `internal/` or `detail/`.
Same-owner use and parent production composition remain allowed. Other clients,
including applications without their own API boundary, need declared source and
provider membership plus a direct dependency on a narrowly granted provider.
This preserves selected test access and explicitly granted sibling/private
composition. Tests receive no blanket exemption, and an uncertain second source
context cannot hide a definite violation in a supported context.

Entry-target checks inspect direct `deps` and `implementation_deps` edges. Edges
within one component and foreign supported API variants pass. Other foreign
implementation dependencies need a narrow grant. Definite test clients can use
testonly helpers without component boundaries. Production-to-testonly enforcement
is left to Bazel, with an explicit not-checked finding.

Only **target-level, explicitly specified finite visibility lists consisting of
literal `//package:__pkg__` entries** supply implementation/private-access grants.
The client's exact package must occur in the list. Public visibility, even mixed
with a matching package entry, does not grant an API bypass. Inherited package
grants, package groups, `__subpackages__`, and unresolved configurable grants are
not checked. Private-header access also
requires an explicit direct code dependency on the provider target. Grant
fixtures cover allowed access and API-bypass rejection; no grant permits an
API include closure into `impl/`.

Cycles use resolved unconditional code edges mapped to their nearest component
owners, including inherited implementation and test targets. Same-owner edges
are discarded. Nested components remain separate, and granted implementation
edges remain in the graph. One deterministic cycle witness is reported per
strongly connected component. Conditional edges are excluded and reported as
not checked; the graph is not a configured-build observation.

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
  evidence for supported access checks, not proof of effective permission.
- `include_prefix`, `strip_include_prefix`, `includes`, `local_includes`, and
  alias `actual` values. Rule evaluation resolves supported include mappings but
  does not expand target aliases.
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
`limitation` on the attribute for not-checked reporting. Other attributes
are intentionally outside the extracted facts. Unknown query record kinds are
retained as repository-level limitations alongside the usable facts. Non-C++
rules retain their kinds without being assigned an architectural role.
Malformed structured tool output,
nonzero tool exits (even with partial stdout), version mismatches, missing pins,
and successful empty query output for active packages raise `OperationalError`.

## Limits and remaining work

The checker assumes a Git repository root and a readable numeric Bazel pin.
Sources must be readable UTF-8. Arbitrary source suffixes, symlinked inputs, C++
semantic analysis, macro expansion, and preprocessor evaluation are outside the
supported subset. `#include_next`, `#import`, `%:include`, computed includes,
ambiguous mappings, unavailable generated headers, and indirect source/header
membership produce coverage limitations. Target aliases and unknown dependency
kinds remain not checked. Unsupported attributes do not disable unrelated checks.

Resolved first-party paths take precedence over external classification.
Unresolved angle includes outside known first-party prefixes are treated as
standard/platform/external headers, including POSIX headers. Quoted external
prefixes can be recognized from external dependency labels; other missing quoted
includes are conservatively reported as not checked. This does not attempt a
complete external header inventory.

Unowned utilities and application wiring do not receive invented component
owners: affected cycle edges are not checked. On the current repository this
also leaves API ownership uncertain for three helper/policy libraries. Effective
visibility, package groups, symbolic-macro permissions, and compilation remain
Bazel's responsibility. Verification entry-point integration is pending; the
checker is currently run separately.

## Verification

From the repository root, run all checker fixtures:

```sh
python3 -m unittest discover -s tools/architecture -p '*_tests.py'
```

To run just the rule, ownership, and command unit suites:

```sh
python3 tools/architecture/includes_tests.py
python3 tools/architecture/dependencies_tests.py
python3 tools/architecture/ownership_tests.py
python3 tools/architecture/check_tests.py
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

The checker integration fixtures also load C++-shaped Starlark declarations with
the pinned Bazel and exercise all four rule failures together, alongside a
nonblocking limitation. A passing parent/nested-component fixture then adds a
conventional untracked component, proving that fresh discovery and queries enforce
its boundary without a registry edit. Repairing its private include leaves only
the computed-include limitation; repeated real loads produce identical facts and
reports. Command fixtures check status codes, explicit/default
repository roots, and failure propagation. Rule fixtures exercise include
chains, guards, comments/strings, conditional paths, mappings, grants, variants,
inherited ownership, nested cycles, and supported checks alongside limitations.
They include conditional private-access grants, `detail/` boundaries, ordinary
and testonly API variants, mutually exclusive selector edges, and supported
nested boundaries beneath unsupported ancestors.

The coverage summary counts resolved first-party include directives, inspected
private-header directives, and evaluated dependency/cycle edges, followed by
not-checked finding counts per rule. These are different units, not coverage
percentages or a count of all possible architectural properties.

Keep the existing repository checks separate:

```sh
python3 tools/verify.py
bazel build //:editor
bazel test //...
bazel test -c dbg //...
```

`tools/verify.py` does not run the new fixtures yet; that integration belongs to
the final implementation step.
