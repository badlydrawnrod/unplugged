# Editor navigation and frame ownership

Consumers depend on `//editor_core:api` and include `editor_core/editor.h`.
The [API](../../include/editor_core/editor.h) owns a document, cursor, preferred
column, and viewport state. [The target](../../BUILD.bazel) depends on document
and shared key APIs. File loading, input acquisition, and drawing belong to
[application composition](../../../applications/editor/main.cpp).

## Byte navigation and wrapping

Text insertion and navigation operate on bytes, including newline and UTF-8
bytes. Width describes the whole viewport: four cells are reserved for the
line-number gutter, with at least one text cell even when width is below five.
Height must be positive and text width representable by the document byte type.
Wrapping counts bytes, without Unicode display-width or grapheme processing.
Empty logical lines remain visible; continuation rows do not repeat the number.

Left and Right traverse byte positions. Home and End select logical-line bounds,
with End before the newline; Ctrl+Home and Ctrl+End select document bounds.
Vertical movement preserves a preferred column across short rows. Crossing a
viewport boundary scrolls one row and selects the new visible row's start.
Page navigation overlaps one row when height permits and stops at document ends.
Commands keep the cursor visible before returning. Unknown keys are no-ops;
Alt+Escape returns false to request application exit.

## Snapshots and dependency direction

`CreateFrame()` returns owned row strings and one-based terminal cursor
coordinates. Snapshot creation does not change editor state; later edits do not
change an existing frame. The cursor column is clamped to the last text cell,
including when the insertion position lies beyond a full row's last byte.
The application emits the frame through the terminal API.

Layout helpers are private to the editor. The document supplies bytes, logical
lines, and wrapped-row views without depending on editor viewport or presentation
state. The [layout-cache proposal](../../../plans/shared-wrapped-row-layout-cache.md)
remains proposed; it adds no current cache, generation, resize, or multi-window
contract. Any implementation must preserve this dependency direction.

## Verification

[Editing and navigation features](../../features/) bind to
[editor API tests](../../tests/editor_tests.cpp). Run
`bazel test //editor_core/tests:editor_tests` and its `-c dbg` variant.
Run `python3 tools/verify.py` from the repository root to validate persisted
scenario bindings and formatting.
