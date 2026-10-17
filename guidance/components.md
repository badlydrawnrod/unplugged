# Creating a component

Use this layout for a cohesive unit with an API and behavior of its own. A
component owns its contract and implementation, with specs, plans, examples,
and tests alongside them when needed. Create directories only for content the
component has.

```text
component/
  README.md                 # current, supported contract
  BUILD.bazel
  include/component/        # public headers
  component.cpp
  specs/                    # temporary specs for proposed changes
  plans/                    # temporary implementation approaches and work slices
  features/                 # acceptance scenarios for observable behavior
  tests/                    # executable checks
```

## Choose ownership

Put a behavior with the component that owns the state and invariants it changes.
Keep its public API narrow, and make other components depend on that API rather
than its implementation. Use an internal subcomponent when a part needs its own
boundary but should remain private to its parent. For a substantial nested
port or subcomponent, use the same `README.md`, `specs/`, `plans/`, `features/`,
and `tests/` convention at that package root. Keep root `specs/` and `plans/`
for cross-component work; identify the participating owners and keep scenarios
with the component that provides the behavior.

## Specify, plan, and implement a change

Use this workflow for both a new feature and a new component:

1. Read the existing contract, public API, specs, and scenarios of the owner
   and affected dependencies. For a new component, first establish the
   responsibility, consumers, and dependency direction it needs.
2. Write or refine a brief/spec in `specs/<topic>.md`. Describe the problem and
   why it matters, desired outcomes, scope and exclusions, constraints, and
   unanswered questions. Describe what success means before choosing how to
   implement it. Include measurable targets for performance or resource goals;
   mark unknown targets as unresolved rather than inventing them.
3. Propose an implementation plan in `plans/<topic>.md` and link it to the spec.
   Describe the technical approach, ownership and API changes, dependencies,
   risks, significant design choices and their tradeoffs, ordered slices of work,
   and how to check each slice. Each slice should deliver a meaningful outcome
   small enough to implement and verify promptly.
4. Define concrete acceptance scenarios for those slices from a consumer's
   perspective. Refine the spec, plan, and examples together as questions arise.
   Use decisions already supplied by the requester and resolve material gaps
   with them before implementing the affected behavior. Keep draft scenarios
   in Markdown until ready to add their executable bindings; every persisted
   `.feature` scenario must have a binding under the
   [acceptance traceability rules](../tools/acceptance/README.md).
5. Take one scenario at a time. Add or revise it in `features/` with a boundary
   test in `tests/`, run the test, and confirm it fails for the intended reason.
   Implement enough behavior to pass, add focused tests where needed, and
   refactor with tests passing before taking the next scenario. Review that
   the assertions actually express the scenario; traceability alone cannot
   establish that.
6. Validate the delivered behavior against the spec's outcomes and constraints.
   Update the README's supported contract before declaring the change done.
   Transfer enduring intent and design rationale from the spec and plan into
   maintained component documentation, following the completion check below.
   Remove the completed spec and plan, and update incoming links to their
   maintained destinations.
   Run the checks required by the repository guide.

For a small, clear change, acceptance scenarios can supply the spec directly,
and a separate spec document or implementation plan may add no value. Keep the
same discipline of defining the outcome and acceptance before implementation.

### Keep the documents useful

| Artifact | Question it answers | Lifetime |
| --- | --- | --- |
| `README.md` | What can callers rely on now, how do they use the API, and why does the component behave this way? | Maintained contract and orientation; links to further rationale |
| Linked design explanation or decision record | Why was this approach chosen, and which constraints and tradeoffs shaped it? | Maintained rationale where more detail is useful |
| `specs/<topic>.md` | Why is this proposed change needed, what should it achieve, and what constrains it? | Temporary; transfer intent and remove after completion |
| `plans/<topic>.md` | How will we implement and verify the change in small slices? | Temporary; remove after completion |
| `features/*.feature` | Which concrete examples define acceptable behavior? | Maintained acceptance scenarios |
| `tests/` | Does the implementation meet those examples and other checked properties? | Executable checks |

Give each active spec a status such as draft or agreed. Make
open questions and deferred scope explicit. Draft specs and candidate plans do
not establish supported behavior. When scope changes, update the spec and its
acceptance examples together. When a spec is superseded, move remaining work to
the active spec and preserve significant rationale before removing the old
working documents. The current README and acceptance tests describe what
callers can rely on today.

Link each plan to its spec and each spec to its acceptance scenarios when they
exist. Plans may link to scenarios or include drafts while refining them. Keep
the persisted scenario wording in `features/` once bindings exist. Avoid copying
whole contracts between documents; use links to the owning source.

### Preserve intent before cleanup

Before deleting a completed spec and plan, check that a future contributor can
start at the owning README and find:

- The problem being solved and the outcome the feature or component serves.
- The constraints and assumptions that shaped its behavior and public boundary.
- The reasons for significant design choices, including relevant alternatives
  considered and the tradeoffs that led to the chosen approach.
- Any conditions that would justify revisiting those choices.

Put concise intent and rationale in the README. When the reasoning needs more
space, use a focused design explanation or decision record beside the owning
component and link it from the README. Create that document when the reasoning
needs it; no extra directory or document is required for every change. For
cross-component decisions, link the shared explanation from the affected
components' READMEs.

Features and tests retain concrete behavioral expectations. The maintained
documentation must also explain why those expectations and boundaries exist.
Git history preserves the working documents, while the current documentation
keeps their important reasoning discoverable. Update that reasoning when goals,
constraints, or design choices change, and distinguish superseded decisions
from the current approach where their history remains useful.

### Start a new component

Create `<new_component>/specs/<topic>.md` for its initial spec, even before the
implementation exists. The plan proposes its public boundary and Bazel targets
and identifies its first acceptance slice. Test that slice through the intended
consumer API rather than private machinery. Establish the initial README
contract and its rationale as part of completing that slice, and extend them
as later slices become supported. A partially implemented component must
describe only the behavior already supported; keep remaining scope visible in
its spec and plan.

## Define the supported boundary

Write the component's `README.md` as its current contract. Describe how to use
the API and the behavior callers may rely on, including important ownership,
lifetime, error, and limitation details. Record desired changes in `specs/` and
their implementation approaches in `plans/`. Update the README when the behavior
is implemented and accepted, as part of completing the change.

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
