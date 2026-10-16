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

Use `internal/` for subcomponents whose APIs are supported only within their
owning component, `impl/` for implementation behind an API, and `detail/` for
unsupported support declarations required by API headers, including template
or inline machinery. Keep a subcomponent's complete class declarations in its
own API headers, alongside its implementation, and in an owning namespace.

Prefer ordinary composition through supported subcomponent APIs. A parent API
header may include an internal subcomponent's API header when a private
by-value member requires its complete type. This makes the declaration visible
to the compiler without making the subcomponent part of the parent's supported
API. Clients of the parent must not directly use those internal declarations
or depend on the internal targets. Keep internal types out of public and
protected signatures, bases, and aliases. Do not include `impl/` headers from
API headers or make clients depend directly on `detail/`.

Do not move a subcomponent's class declarations into parent-owned layout
headers and introduce alias-only API wrappers solely to avoid transitive
visibility. Use PImpl when hiding representation, reducing rebuild dependencies,
or preserving a binary interface justifies the allocation, indirection, and
explicit copy/move policy.

See the [Bazel guidance](bazel.md) for expressing these boundaries in targets
and visibility. Formatting and compiler rules are checked by the repository's
verification and build commands; see the [repository guide](repository.md).
