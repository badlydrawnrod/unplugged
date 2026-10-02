# Repository Guidelines

## Project Structure & Modules

This is a small C++23 terminal editor. The application entry point and editor loop are in `editor.cpp`; document and editing primitives live in root-level pairs such as `document.{h,cpp}`, `gap_buffer.{h,cpp}`, and `line_starts.{h,cpp}`. Tests are kept beside the code as `*_tests.cpp`. `old_20260924/` contains archived experiments; do not add new production code there. CMake downloads GoogleTest and Microsoft GSL into the build tree.

## Build, Test, and Run

Configure and build from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/editor
```

The build uses CMake 3.25+, C++23, and Ninja or another supported generator. Run `./build/editor [file]` to open a file. Tests can also be run individually, for example `./build/gap_buffer_tests`.

## Coding Style & Naming

Follow `.clang-format` (Google-derived C++ style, two-space indentation, 80-column limit, left-aligned pointers). Keep declarations in `.h` files and implementations in `.cpp` files. Use lowercase `snake_case` filenames and identifiers; test files use the matching module name plus `_tests.cpp`. The build treats `-Wall -Wextra -Wpedantic` warnings as errors, so keep changes warning-clean.

## Testing Guidelines

Tests use GoogleTest and are registered with CTest. Add or update a `*_tests.cpp` suite for behavior changes, keeping tests focused on the corresponding module. Run `ctest --test-dir build --output-on-failure` after building; no coverage threshold is configured.

## Commits & Pull Requests

Git history is unavailable in this workspace, so no established commit subject pattern could be verified. Use a short imperative subject (for example, `Add wrapped row range tests`). Pull requests should explain the behavior change, note relevant test results, and link an issue when applicable. Include terminal screenshots only for visible UI changes.

## Configuration Notes

CMake FetchContent requires network access on the first configure unless dependencies are already cached under `build/_deps`. Keep generated build files out of source changes.
