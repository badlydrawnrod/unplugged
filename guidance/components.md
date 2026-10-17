# Creating a component

Use this layout for a cohesive unit with an API and behavior of its own. A
component owns its contract and implementation, with examples, tests, and
proposals alongside them when needed. Create directories only for content the
component has.

```text
component/
  README.md                 # current, supported contract
  BUILD.bazel
  include/component/        # public headers
  component.cpp
  features/                 # significant observable behavior
  tests/                    # executable checks
  plans/                    # proposed component work
```

## Choose ownership

Put a behavior with the component that owns the state and invariants it changes.
Keep its public API narrow, and make other components depend on that API rather
than its implementation. Use an internal subcomponent when a part needs its own
boundary but should remain private to its parent. For a substantial nested
port or subcomponent, use the same `README.md`, `features/`, `tests/`, and
`plans/` convention at that package root. Keep root `plans/` for cross-cutting
proposals.

## Define the supported boundary

Write the component's `README.md` as its current contract. Describe how to use
the API and the behavior callers may rely on, including important ownership,
lifetime, error, and limitation details. Keep proposals in `plans/`; do not
describe proposed behavior as supported behavior.

Put public headers in `include/<component>/` and expose them through an `:api`
Bazel target. Give the target only the visibility its legitimate consumers
need. For example:

```bzl
cc_library(
    name = "api",
    srcs = ["component.cpp"],
    hdrs = ["include/component/component.h"],
    strip_include_prefix = "include",
    visibility = [
        "//client:__pkg__",
        "//component/tests:__pkg__",
    ],
    deps = ["//other_component:api"],
)
```

Consumers should depend on `//component:api`, not implementation targets.
List the actual consumer packages or use a deliberate visibility group when
many consumers share access. See the [Bazel guidance](bazel.md) for target and
implementation boundaries.

## Check behavior

Add focused tests under `tests/`, with test targets depending on the supported
API. Test observable behavior through that API; use implementation-focused
tests only when an implementation property itself needs checking. Follow the
[C++ guidance](cpp.md) for test names, headers, and implementation structure.

When a change adds or changes significant externally observable behavior, add a
deterministic scenario under `features/` and bind it to an executable test.
Keep scenarios independent of implementation details. Read the
[acceptance traceability guide](../tools/acceptance/README.md) for this
repository's scenario format and binding rules.

Adapt Bazel labels and check commands to the repository. The directory and
ownership conventions can be reused in other C++/Bazel repositories without a
central component registry.
