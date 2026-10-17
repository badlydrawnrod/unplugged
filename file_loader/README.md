# File loading

## Load a file

Depend on `//file_loader:api` and call
[`file_loader::Load`](include/file_loader/load.h) to return file contents as
bytes for the editor application.

## Size limit and errors

The supported document-size limit is `kMaxDocumentBytes`. Loading a larger
file throws `std::length_error` before allocating its contents. The API does
not specify how open or read failures are reported, so callers must not infer
that behavior from the current implementation.

The [document-size scenario](features/document_size.feature) and
[loader tests](tests/) cover rejection of oversized input.
