# Improve component discoverability

Improve the documentation and component-creation path in this repo. The aim is
that a human or coding agent can add a component and find its contract, behaviour
examples, tests, and unfinished plans by following directory conventions. Do not
introduce a central component registry that must be updated for each new
component. Keep this a documentation and verification change; preserve product
behaviour.

## Work

- [x] Inspect the current repository and its contributor instructions before
  editing.
- [x] Establish a discoverable component layout.
  - [x] Use `<component>/README.md` as the component's current, supported
    contract.
  - [x] Keep public headers under `include/<component>/`, significant
    behaviour examples under `features/`, tests under `tests/`, and unfinished
    component work under `plans/`.
  - [x] Create these directories only when needed.
  - [x] Apply the same contract-location convention to a substantial nested
    port or subcomponent.
  - [x] Keep root `plans/` for genuinely cross-cutting work.
- [ ] Move and clarify existing contracts.
  - [x] Move existing documents from `<component>/docs/specs/` to the owning
    component's `README.md`.
  - [x] Move the byte-source contract to its port root.
  - [x] Update relative links throughout the repository and remove the old
    copies.
  - [ ] Give other top-level public components concise READMEs based on their
    supported APIs and established behaviour.
  - [ ] Record ambiguous implementation details that should not become
    approved contracts for separate reporting; do not turn them into contract
    statements.
- [ ] Write one reusable component-creation guide at
  `guidance/components.md`.
  - [ ] Link the guide from `AGENTS.md`.
  - [ ] Show a minimal directory example.
  - [ ] Explain how to choose ownership, create a public `:api` Bazel target
    with narrow visibility, and add tests through the supported API.
  - [ ] Explain when to add a feature scenario for significant observable
    behaviour.
  - [ ] Link to the existing C++ and Bazel guidance instead of repeating it.
  - [ ] Keep the guide suitable for another C++/Bazel repository using this
    layout.
- [ ] Make navigation local.
  - [ ] Adjust the root README so its component map is clearly a set of common
    starting points, not an exhaustive registry.
  - [ ] State that the owning component's README is the contract and that
    neighbouring `features/`, `tests/`, and `plans/` contain examples, checks,
    and proposals.
  - [x] Move component-specific plans beside their owner and update their
    links.
  - [ ] Keep proposed behaviour clearly separate from the current contract.
- [ ] Add concise writing guidance to the reusable engineering guidance.
  - [ ] Align it with [ISO 24495-1:2023](https://www.iso.org/standard/78907.html):
    write for the reader's task; make needed information relevant, findable,
    understandable, and usable.
  - [ ] Recommend descriptive headings, direct language, concrete examples,
    and defined terms where needed.
  - [ ] Apply [Diátaxis](https://diataxis.fr/start-here/) pragmatically: choose
    tutorial, how-to, reference, or explanation according to the reader's need.
  - [ ] Describe a component README as primarily a contract/reference with
    enough orientation to use it; do not require all four document types.
  - [ ] Do not claim formal ISO conformance.
- [ ] Make the placement convention hard to forget.
  - [ ] Add a small verification rule requiring `README.md` for each top-level
    public library component identified by a `BUILD.bazel` and `include/`
    directory.
  - [ ] Use filesystem convention rather than a maintained list or Bazel
    parser.
  - [ ] Add focused tests for the rule and run it through the existing
    verification entry point.
  - [ ] Check presence only; do not attempt to score prose quality.
- [ ] Run `python3 tools/verify.py` and the relevant Bazel checks.
- [ ] Report what moved, how someone adds and finds a new component, which
  checks passed, and any unresolved contract decisions.

## Constraints

- Keep the change focused.
- Do not add a documentation generator, a Gherkin runner, or a metadata
  registry.
- Preserve product behaviour.
