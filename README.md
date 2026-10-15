# Unplugged

Unplugged is a small C++23 terminal editor. The repository is organised so you can find the part responsible for a behaviour, read its contract, and check a change.

The editor opens a file and lets you edit it in memory. It does **not** save changes. Navigation and wrapping count bytes rather than Unicode display cells.

## Run the editor

You need Linux, Bazelisk, and a compiler that supports C++23. From the repository root, run:

```sh
bazel run //:editor -- path/to/existing-file
```

Press **Alt+Escape** to exit. Use a file you can read; changes made in the editor will be lost when you exit.

## Find the right part of the repo

| If you are changing… | Start here |
| --- | --- |
| Document bytes, lines, edits, or wrapped views | [Document contract](document/docs/specs/document.md) |
| Editing commands, navigation, or the displayed frame | [Editor contract](editor_core/docs/specs/editor.md) |
| Key decoding | [Key decoder](key_decoder/) |
| Byte acquisition and its adapter contract | [Byte-source contract](key_decoder/ports/byte_source/docs/specs/byte_source.md) |
| Terminal input, output, or restoration | [Terminal contract](terminal/docs/specs/terminal.md) |
| File loading | [File loader](file_loader/) |
| Program startup and component wiring | [Editor application](applications/editor/main.cpp) |

Each component’s public headers describe its supported API. Its `features/` directory holds significant acceptance behaviour, and its `tests/` directory holds executable checks.

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

The format check requires clang-format 21.1.8. [CI](.github/workflows/ci.yml) runs these checks on pushed changes and pull requests.

For repository conventions, read [AGENTS.md](AGENTS.md). For the supported scenario syntax and test annotations, read the [acceptance traceability guide](tools/acceptance/README.md). Work that is still being considered lives under [plans/](plans/); component contracts describe behaviour that should remain true after a plan is complete.