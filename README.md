# Unplugged

Unplugged is a small C++23 terminal editor. The repository is organised so you can find the part responsible for a behaviour, read its contract, and check a change.

The editor opens a file and lets you edit it in memory. It does **not** save changes. Navigation and wrapping count bytes rather than Unicode display cells.

## Run the editor

You need Linux, Bazelisk, and a compiler that supports C++23. From the repository root, run:

```sh
bazel run //:editor -- path/to/existing-file
```

Press **Alt+Escape** to exit. Use a file you can read; changes made in the editor will be lost when you exit.

## Find a starting point

These are common starting points, not a complete component registry. If your
change does not fit a row, follow the owning code and its component README.

| If you are changing… | Start here |
| --- | --- |
| Document bytes, lines, edits, or wrapped views | [Document contract](document/README.md) |
| Editing commands, navigation, or the displayed frame | [Editor contract](editor_core/README.md) |
| Shared key values | [Key contract](key/README.md) |
| Key decoding | [Key decoder contract](key_decoder/README.md) |
| Byte acquisition and its adapter contract | [Byte-source contract](key_decoder/ports/byte_source/README.md) |
| Terminal input, output, or restoration | [Terminal contract](terminal/README.md) |
| Checked descriptor I/O | [Terminal I/O contract](terminal_io/README.md) |
| File loading | [File loader contract](file_loader/README.md) |
| Program startup and component wiring | [Editor application](applications/editor/main.cpp) |

The owning component's `README.md` is its current supported contract. Public
headers under `include/<component>/` define the supported API. Where present,
neighbouring `specs/`, `plans/`, `features/`, and `tests/` directories contain
temporary change specs, temporary implementation plans, acceptance scenarios,
and executable checks. The README describes what is supported now and explains
its intent, linking to further design rationale where needed.

## Make and check a change

1. Find the component that owns the behaviour. Read its current contract,
   public API, relevant specs, and acceptance scenarios. For a new component,
   establish its responsibility and boundary using the
   [component guide](guidance/components.md).
2. Write or refine a brief/spec describing the problem, desired outcomes,
   scope, and constraints in the owner's `specs/` while the change is in progress.
   For a small, clear change, acceptance scenarios can supply the
   spec without a separate document.
3. Propose an implementation plan in the owner's `plans/` when needed. Define
   concrete acceptance scenarios for its slices of work. Refine the spec, plan,
   and scenarios together until the approach and definition of done are clear.
   See the [full workflow](guidance/components.md#specify-plan-and-implement-a-change).
4. Implement one scenario at a time. Add or revise the component's `.feature`
   scenario and its boundary test through the supported API, then confirm the
   test fails for the expected reason. See the
   [acceptance traceability guide](tools/acceptance/README.md) for scenario
   syntax and test bindings.
   Implement the behaviour, then refactor with tests passing. Keep focused
   implementation tests for properties that are not acceptance behaviour.
5. Check the result against the intended outcomes. Update the README's contract
   to match the implemented and accepted behaviour before calling the change
   done. Preserve the problem, goals, constraints, and reasons for significant
   design choices in the owning README or a linked, maintained explanation.
   Remove the completed spec and plan only after transferring that intent and
   updating incoming links to their maintained destinations.
6. Run the checks from the repository root:

```sh
python3 tools/verify.py
bazel build //:editor
bazel test //...
bazel test -c dbg //...
```

The format check requires clang-format 21.1.8. [CI](.github/workflows/ci.yml)
runs these checks on pushed changes and pull requests.

## Guidance

Start with [AGENTS.md](AGENTS.md) for contributor instructions. It links to the
reusable [engineering principles](guidance/engineering.md), language-specific
[C++ guidance](guidance/cpp.md), [Bazel guidance](guidance/bazel.md), and the
[Unplugged repository guide](guidance/repository.md). The
[acceptance traceability guide](tools/acceptance/README.md) describes supported
scenario syntax and test annotations.

Use root `specs/` and `plans/` directories for cross-component work. Otherwise,
keep them beside their owner. The editor's
[layout spec](editor_core/specs/responsive-wrapped-row-layout.md) records its
desired outcomes; the [layout-cache plan](editor_core/plans/shared-wrapped-row-layout-cache.md)
records a candidate implementation approach. Both remain drafts.
