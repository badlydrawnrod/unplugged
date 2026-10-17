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
neighbouring `features/`, `tests/`, and `plans/` directories contain behavior
examples, executable checks, and proposals. A proposal remains separate from
the current contract until its behavior is implemented and accepted.

## Make and check a change

1. Find the component that owns the behaviour.
2. Read its contract, public API, and relevant acceptance scenarios.
3. Change the behaviour and its tests. Update the contract or scenarios when the intended behaviour changes.
4. Run the checks from the repository root:

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

Cross-cutting work that is still being considered lives under [plans/](plans/).
Component-specific proposals live beside their owner, such as the
[editor layout-cache proposal](editor_core/plans/shared-wrapped-row-layout-cache.md).
Component READMEs describe supported behaviour; proposals become contracts only
after their behavior is implemented and accepted.
