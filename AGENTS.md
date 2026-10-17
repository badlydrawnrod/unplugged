# Contributor guidance

Apply the [engineering principles](guidance/engineering.md) to every change.
For C++ work, also follow the [C++ guidance](guidance/cpp.md); for Bazel
work, follow the [Bazel guidance](guidance/bazel.md). The
[repository guide](guidance/repository.md) has project structure, commands,
and checks. Follow the [component-creation guide](guidance/components.md)
when adding a component.

Before changing behavior, find its owning component, read its public API and
[component contract](README.md#find-a-starting-point), and update
relevant tests and persisted scenarios. Keep durable behavior in the owning
component's README contract. Read the
[acceptance traceability guide](tools/acceptance/README.md) when adding or
changing feature scenarios or bindings.

Run `python3 tools/verify.py` and the relevant Bazel build and tests. For a
repository-wide change, use the full commands in the repository guide. Keep
changes focused and preserve unrelated working-tree edits.
