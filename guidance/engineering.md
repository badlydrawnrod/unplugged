# Engineering principles

These principles are language-independent and can be reused in other
repositories. Repository-specific conventions and executable checks belong in
that repository's own guidance and tooling.

## Preserve intent

Implement the requested behavior, not merely the requested code shape. Prefer
designs that make intent explicit, local, and easy to verify. Where practical,
express rules through types, assertions, tests, or other mechanical checks
rather than relying only on prose.

## Components and boundaries

Design small, cohesive components with narrow, stable APIs and explicit
dependency direction. Treat boundaries recursively: a component can contain
subcomponents with stable APIs that remain private to their parent. Keep
implementation details behind supported APIs, and avoid widening access just
because a consumer currently needs an internal detail. Reconsider the boundary
when that need repeats.

## Verification

Use the strongest appropriate way to express correctness:

1. Type-system and compile-time guarantees.
2. Runtime assertions for local invariants.
3. Behavioral tests through stable component APIs.
4. Focused implementation tests when properties of the implementation itself
   matter.

Test components through the narrowest stable API exposed to their consumers.
Boundary tests should verify externally observable behavior and remain valid
when the implementation changes without changing the contract. Avoid coupling
them to internal call sequences, private structure, or incidental representation.
Do not expose implementation details only to make tests easier.

## Acceptance behavior

Persist significant externally observable behavior as deterministic scenarios
owned by the component that provides it. Keep scenarios independent of
implementation details and verify them through the supported API. Keep each
scenario traceable to an executable test. Use ordinary focused tests for
implementation properties that are not acceptance behavior.

## Change discipline

Work in small, coherent increments. Keep the build and relevant checks green as
the work proceeds. Refactor before adding further complexity, preserve or
improve component boundaries, and avoid broader dependencies than the task
requires. When existing code conflicts with these principles, improve it locally
where safe instead of propagating the coupling.
