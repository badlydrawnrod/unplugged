# Contributor guidance

Apply the [engineering principles](guidance/engineering.md) to every change.
For C++ work, also follow the [C++ guidance](guidance/cpp.md); for Bazel
work, follow the [Bazel guidance](guidance/bazel.md). The
[repository guide](guidance/repository.md) has project structure, commands,
and checks. Follow the [component-creation guide](guidance/components.md)
when adding a component.

Before changing behavior, find its owning component and read its public API,
[current contract](README.md#find-a-starting-point), and relevant specs and
scenarios. Follow the [change workflow](guidance/components.md#specify-plan-and-implement-a-change):
keep durable intent in component-owned `specs/`, implementation approaches in
temporary `plans/`, and acceptance scenarios in `features/` with executable test
bindings. For small changes, scenarios can supply the spec without a separate
document or plan. Implement one scenario at a time, starting with a test that
fails for the expected reason. Update the owning README's supported contract as
part of completing the change; retain the spec and remove the completed plan.
Read the
[acceptance traceability guide](tools/acceptance/README.md) when adding or
changing feature scenarios or bindings.

Run `python3 tools/verify.py` and the relevant Bazel build and tests. For a
repository-wide change, use the full commands in the repository guide. Keep
changes focused and preserve unrelated working-tree edits.
