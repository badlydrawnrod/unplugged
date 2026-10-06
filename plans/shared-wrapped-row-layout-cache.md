# Shared wrapped-row layout cache


Status: design proposal; implementation has not started.
Reviewed: 2026-10-06.

## Review notes and dependencies

The repeated layout scans described below are present in `editor.cpp`, notably
in `TotalWrappedRowCount`, `TopAnchorForRow`, and `KeepCursorVisible`. The current
application has one window with fixed dimensions; multi-window ownership,
resize handling, and inactive-width LRU eviction are proposed future behavior.

This proposal overlaps with
[the engineering alignment plan](align-with-engineering-guidance.md), which
extracts a testable editor component and establishes component boundaries.
Coordinate the cache API with that work. Sharing layout per document does not
necessarily require placing presentation-specific cache machinery inside the
`Document` storage model; choose ownership and dependency direction explicitly.
Use document identity rather than a filename alone to identify shared state.

The proposed generation invalidation prevents stale reads, but a full rebuild
on the first lookup after every edit still scans the document on each editing
keystroke. Reusing a layout primarily helps repeated lookups and navigation
between edits. Meeting the stated editing-performance goal requires a measured
rebuild policy, such as incremental updates; suffix rebuilding can still be
linear for edits near the start of a document.

Before implementing:

- [ ] Measure representative large-file editing and navigation costs, and
  define the performance and memory targets.
- [ ] Decide whether the first increment covers only the current single-window
  behavior or also introduces multi-window and resize support.
- [ ] Define generation changes for successful edits, no-op edits, and failed
  edits, and prevent partially rebuilt layouts from being published as valid.
- [ ] Specify layout/view lifetimes and active-width registration and release
  semantics if implementing multi-window support.
- [ ] Choose the edit-update strategy needed to meet the measured goal.
- [ ] Confirm whether inactive-width LRU reuse earns its complexity for the
  supported workflow. The active-set memory bound below is not a fixed byte
  ceiling; memory also grows with line count and distinct active widths.

The following design is retained as a proposal, subject to those decisions.
Keep this plan updated while implementing it, and remove it after completion
once enduring contracts are captured in code, tests, and acceptance scenarios.

## Goal

Stop doing full-document wrapped-row scans on every keystroke. Keep it cheap enough for embedded hardware and correct when multiple windows are showing the same file.

## Problem

The editor currently recomputes wrapped-row layout by scanning the whole document during scroll and cursor checks. That gets expensive fast when the file is large, when the window is resized, or when two windows show the same file.

## The rule

One file gets one shared wrapped-row layout cache, not one cache per window. The cache is keyed by wrap width, and every entry must be versioned against the document generation.

## Invariant block

- Layout cache is shared per document, not per window.
- Layout cache key is: document + wrap width.
- A cache entry is valid only when width matches and generation matches.
- Window state is local: `top_row`, viewport, cursor, preferred column.
- Document state is shared: wrapped-row layout, line row counts, cumulative row offsets.
- Same file + same width + same generation => same layout entry.
- Same file + different width => different layout entry.
- Same file + edit => generation changes, so old layout entries become stale.
- Resize changes width, not document identity, so the resize path picks a different width entry or rebuilds it.
- Keep one entry for each distinct width used by an open window, plus a bounded set of inactive entries; only the inactive-entry limit affects reuse performance.

## Small design

Use a document-owned cache keyed by wrap width. Each distinct width used by an open window has one active layout entry; windows at the same width share that entry. Track how many windows use each width so an entry remains active until the last window at that width changes size or closes.

Keep active entries while their windows are open. Use a small LRU set only for inactive entries, so recently used widths can be reused after a resize without allowing old resize history to grow without bound. When memory pressure or the inactive-entry limit requires eviction, evict the least recently used inactive entry. If a window requests an evicted width, rebuild it and mark it active.

This means memory grows with the number of distinct widths currently needed by open windows, plus a small bounded number of inactive layouts. That is necessary to avoid repeatedly rebuilding layouts that concurrent windows are using. If a hard memory ceiling requires a cap below the active set, the editor can still rebuild on demand, but may pay repeated wrapping costs while switching among windows; treat that as an explicit fallback policy.

Each layout entry stores:

- `wrap_width`
- `document_generation`
- `rows_per_line`
- `row_offsets`
- `total_rows`

If the width matches and the generation matches, the entry is valid. A lookup by an open window marks the width active. When the last window stops using a width, mark its entry inactive and update its LRU position. If an entry is stale or absent, rebuild it for the requested width and current generation.

## How it works

### On edit

- bump `document_generation`
- mark all cached width entries stale (or let the generation check detect staleness on lookup)
- when a window asks for a layout next, rebuild that width for the new generation, using the relevant suffix if incremental rebuilding is supported

### On resize

- compute the new wrap width
- release the window's reference to its old width; if no other window uses that width, mark the entry inactive
- ask for the cache entry for the new width and register the window as using it
- on a cache hit, use the shared entry; if the entry is stale, rebuild it
- on a cache miss, build an entry for that width; evict an inactive LRU entry first if the inactive-entry limit is full
- then use it for that window

### On multiple windows

All windows share the same document cache.

This is important: if two windows show the same file and the same width, they reuse the same wrapped-row layout data. They still keep their own `top_row`, cursor, and viewport state, but the expensive wrapping math is shared.

If the windows have different widths, each distinct open-window width has an active width-specific entry under the same document generation rules. Multiple windows at one width share that entry. Resizing or closing a window releases its use of the old width; only when the last window releases that width can its entry become an LRU eviction candidate.

## Memory budget

Memory is bounded by the active distinct widths plus a configured maximum number of inactive LRU entries. Arbitrary historical widths do not accumulate after windows resize or close.

Roughly:

- one layout per distinct width currently used by an open window, plus at most `max_inactive_widths` inactive layouts
- each layout is O(number of lines in file)
- same-width windows add no duplicate layout storage
- closing or resizing the last window at a width makes that layout eligible for LRU eviction

This is a much better trade than repeated whole-document scans on every keypress.

## Correctness requirements

The cache is valid only when both of these are true:

- width matches
- document generation matches

That means a stale cache cannot be used after edits or after a resize to a different width.

## Implementation order

1. Add a document cache keyed by width, with active-window use counts, a bounded inactive LRU set, and a document generation counter.
2. Route `WrappedRowCountForLine`, `TotalWrappedRowCount`, and `TopAnchorForRow` through the cache.
3. Add generation checks before using any cached width layout.
4. Rebuild the layout entry when stale.
5. Update resize logic to switch to the correct width-specific layout.
6. Validate with focused tests for:
   - wrapped-row counts
   - EOF and last-line cursor visibility
   - resize across widths
   - multiple windows sharing one file at the same width
   - multiple windows sharing one file at different widths, keeping each active width layout reusable
   - multiple windows sharing one width, with the entry becoming inactive only after the last window releases it
   - an inactive evicted width rebuilds correctly when requested again

## Proposed approach

Share layouts per document and width, validate them against document generation,
and retain active widths while bounding inactive entries. Validate the update
strategy and memory cost against the targets established above.
