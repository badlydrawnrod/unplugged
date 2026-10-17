# File loading

`//file_loader:api` provides
[`file_loader::Load`](include/file_loader/load.h), which returns file contents
as bytes for the editor application. The supported document-size limit is
`kMaxDocumentBytes`; loading a larger file throws `std::length_error` before
allocating its contents.

The [document-size scenario](features/document_size.feature) and
[loader tests](tests/) cover rejection of oversized input. The API does not
specify how open or read failures are reported; callers should not infer that
behavior from the current implementation.
