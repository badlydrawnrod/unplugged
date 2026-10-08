# Acceptance traceability

Run `python3 tools/acceptance/check_traceability.py` from the repository root,
using the script's absolute path when working elsewhere. The script resolves
its repository from its own location. It checks active tracked and untracked
files from Git, excludes ignored output, skips deleted paths, and does not follow
file or directory symlinks. It scans `.feature` files and C++ `.cpp`, `.cc`,
`.cxx`, `.h`, and `.hpp` files. No third-party Python packages are required.

Each persisted scenario must have at least one executable GoogleTest binding.
Several tests may bind the same scenario. Scenario identity is its normalized
repository-relative feature path plus its exact title; the same title may appear
in different feature files. Duplicate identities within a feature are rejected.
Titles are case-sensitive; surrounding whitespace is ignored, but internal
whitespace and punctuation must match.

## Supported feature syntax

The checker deliberately supports the repository's English Gherkin subset:

- One nonempty `Feature:` title before any scenarios.
- Optional free-form feature description before the first scenario.
- Nonempty `Scenario:` titles, each with at least one step.
- Steps beginning with `Given`, `When`, `Then`, `And`, `But`, or `*` and nonempty
  step text. Step order and meaning are verified by the bound behavioral tests.
- Blank lines and whole-line `#` comments.

It rejects unsupported constructs with a file and line diagnostic, including
`Background`, `Rule`, `Scenario Outline`/`Scenario Template`, `Examples`,
`Example`, tags, data tables, doc strings, and language directives. Scenario
prose and unrecognized keyword headers are rejected. Extend the parser and its
fixtures deliberately before introducing additional Gherkin forms.

## Supported bindings

Place these two standalone line comments immediately above a test definition:

```cpp
// Feature: document/features/document_editing.feature
// Scenario: Inserting text and a newline preserves surrounding content
TEST(DocumentTest, InsertsTextAndNewline) {
  // Exercise the supported component API.
}
```

`TEST`, `TEST_F`, and `TEST_P` are supported. Their two arguments must be ordinary
C++ identifiers; arguments and the opening brace may span multiple lines. There
must be no blank line, intervening comment, or declaration between the annotation
pair and the first line of the test macro. Feature paths use forward slashes,
contain no `.` or `..` segments, and must reference a discovered `.feature` file.

Malformed, missing, reversed, or detached annotation comments fail the check.
Ordinary implementation tests do not need annotations. C++ comments and string
literals (including raw strings) do not supply test definitions or annotations.
The checker recognizes source definitions, not preprocessor configurations or
GoogleTest runtime registration; it does not prove that a binding's assertions
implement the scenario. Build and run the component tests as well. Custom test
macros are unsupported and cannot serve as annotated bindings.

## Verification

`python3 tools/acceptance/check_traceability_tests.py` runs isolated temporary
fixtures for successes, failures, discovery, and invocation from another working
directory. Failures report feature/test paths and line numbers; the checker exits
with status 1 when traceability or supported syntax is invalid.

`python3 tools/verify.py` runs formatting, repository traceability, and these
fixture tests, attempting all three checks and failing if any fails. It also
accepts `--clang-format /path/to/clang-format`. The entry point works from any
working directory when invoked by its absolute path. Continue running Bazel
build/default/debug tests separately; no new runner dependency or CI is needed.
