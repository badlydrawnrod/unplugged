# C++ guidance

Use idiomatic C++23 and follow local conventions when they are more specific.
Prefer types that make invalid states unrepresentable, standard ranges and
algorithms when they improve clarity, explicit ownership, and small modules
with clear dependencies. Use ports and adapters when external dependencies
would otherwise prevent deterministic testing of core behavior.

## Names and formatting

Use the repository's Google-derived naming conventions:

- `PascalCase` for types and functions.
- `snake_case` for variables and parameters.
- `snake_case_` for class data members.
- `kPascalCase` for constants.
- `snake_case` for filenames and namespaces.

Follow `.clang-format` and compiler diagnostics. Declarations belong in headers;
implementations belong in `.cpp` files. Test files use the owning module name
plus `_tests.cpp`.

## Headers and implementation

Use `internal/` for code not exposed outside its owning component, `impl/` for
implementation behind an API, and `detail/` for template or inline machinery
required by API headers but not supported as an API. Do not expose internal
implementation through repository-public headers, include `impl/` headers from
API headers, or make consumers depend directly on `detail/`.

See the [Bazel guidance](bazel.md) for expressing these boundaries in targets
and visibility. Formatting and compiler rules are checked by the repository's
verification and build commands; see the [repository guide](repository.md).
