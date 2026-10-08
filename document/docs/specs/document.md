# Document ownership, edits, and views

Consumers depend on `//document:api` and include logical `document/*.h` paths.
The supported declarations are in [include/document/](../../include/document/);
[the Bazel targets](../../BUILD.bazel) enforce storage ownership.

## Bytes and edits

Offsets and logical-line ordinals are zero-based. Every document has a line
starting at byte zero, including an empty document. Each newline byte starts a
following logical line, so a trailing newline creates an empty final line.
`LineFromPos()` accepts the end position; `At()` requires an existing byte.

`Edit(pos, delete_count, insert_bytes)` replaces a valid byte interval. Positions
may equal the document length, but deletion must stay within the document.
Results exceeding `kMaxDocumentBytes` throw `std::length_error`. Allocation
failures propagate without changing bytes or line queries; previously acquired
views remain usable after a failed edit, and a subsequent edit may succeed.
Deletion and empty no-op edits need no allocation. These guarantees concern the
observable document, without prescribing allocation counts or storage layout.

## Borrowed views

`DocumentView` stores a document reference and a fixed byte interval; it owns no
bytes. Logical-line ranges borrow the document and retain line bounds. A wrapped
range borrows the document and retains its source interval and width; its
iterators also borrow the range object. Keep these owners alive while using the
views or iterators. A view does not track an edit as a moving text selection.
Reacquire ranges and iterators after successful mutation, assignment, or moving
the document; there is no snapshot or automatic invalidation check. Copying a
view preserves its borrowed owner. An owned byte copy is needed for a snapshot.

Logical-line views include their terminating newline. Wrapped rows omit that
newline, count bytes at a positive width, and yield one empty row for an empty
line. Exact-width content produces no extra row. Document wrapping supplies byte
views; gutters, viewport state, and cursor rendering belong to the editor.

## Inline representation and contract configuration

Document owns storage inline. Public headers require complete unsupported
[detail declarations](../../detail/), preserving inline ownership without a
PImpl allocation. Consumers must not use those declarations or depend directly
on layout targets or internal storage APIs. Storage implementations remain behind
restricted `:api` boundaries.

`//document:test_api` exposes the same supported API only to document tests.
Its shared `//:contract_test_mode` dependency propagates `CONTRACT_EXCEPTIONS`
through document and storage translation units in debug builds. This includes
inline definitions and conditional `noexcept`; mixing configurations is invalid.
With assertions enabled and the debug test policy active, contracts throw
`std::logic_error`; otherwise enabled contracts abort. With `NDEBUG`, contract
checks are disabled. Size-limit and allocation failure reporting remain ordinary
runtime behavior.

## Verification

[Features](../../features/) capture byte edits, empty documents, wrapping, and
edit failures. Their executable annotations live in [API tests](../../tests/),
including the existing-view failure check and public-header compile assertions.
Run `bazel test //document/tests:all` and
`bazel test -c dbg //document/tests:all`. Run `python3 tools/verify.py` from the
repository root for formatting and acceptance traceability, and retain the full
repository default/debug suites for cross-component configuration checks.
