# Repository Guidelines

## Project Structure & Modules

This is a small C++23 terminal editor. The application entry point and editor loop are in `editor.cpp`; document and editing primitives live in root-level pairs such as `document.{h,cpp}`, `gap_buffer.{h,cpp}`, and `line_starts.{h,cpp}`. Tests are kept beside the code as `*_tests.cpp`. `old_20260924/` contains archived experiments; do not add new production code there. Bazel dependencies are declared in `MODULE.bazel`.

## Build, Test, and Run

Install Bazelisk, then run commands from the repository root. Bazelisk reads
the pinned Bazel version from `.bazelversion`; dependencies are declared in
`MODULE.bazel` and locked in `MODULE.bazel.lock`.

```sh
bazel build //:editor
bazel test //...
bazel run //:editor -- [file]
```

Run one test target with `bazel test //:gap_buffer_tests` (or another
`*_tests` target). The project uses C++23, GoogleTest, and Microsoft GSL; the
editor relies on POSIX terminal APIs.

## Coding Style & Naming

Follow `.clang-format` (Google-derived C++ style, two-space indentation, 80-column limit, left-aligned pointers). Keep declarations in `.h` files and implementations in `.cpp` files. Use lowercase `snake_case` filenames and identifiers; test files use the matching module name plus `_tests.cpp`. The build treats `-Wall -Wextra -Wpedantic` warnings as errors, so keep changes warning-clean.

## Testing Guidelines

Add or update a `*_tests.cpp` suite for behavior changes, keeping tests focused
on the corresponding module. Run `bazel test //...`; no coverage threshold is
configured. Test targets relax only the
`sign-compare` error for GCC 15 diagnostics emitted by GoogleTest assertion
templates.

## Commits & Pull Requests

Git history is unavailable in this workspace, so no established commit subject pattern could be verified. Use a short imperative subject (for example, `Add wrapped row range tests`). Pull requests should explain the behavior change, note relevant test results, and link an issue when applicable. Include terminal screenshots only for visible UI changes.

## Configuration Notes

Keep generated build files out of source changes.
