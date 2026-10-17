# File loading

## Load a file

Depend on `//file_loader:api` and call
[`file_loader::Load`](include/file_loader/load.h) to return file contents as
bytes for the editor application.

## Size limit and errors

The supported document-size limit is `kMaxDocumentBytes`. Loading a larger
file throws `std::length_error` before allocating its contents. A valid empty
file loads successfully as an empty byte buffer. If a file cannot be opened,
sized, or read, `Load()` throws `std::runtime_error` with the file path and
failure stage. The editor application reports this error and exits without
entering terminal mode.

The [file-opening](features/file_opening.feature) and
[document-size](features/document_size.feature) scenarios, bound to the
[loader tests](tests/), cover these outcomes.
