# Responsive wrapped-row layout

Status: draft; implementation has not started. Scope and measurable targets
remain unresolved.

Current contract: [editor navigation and frames](../README.md).
Candidate implementation: [shared wrapped-row layout cache](../plans/shared-wrapped-row-layout-cache.md).

## Problem and desired outcome

The editor repeatedly scans document lines to compute wrapped-row counts and
locate the viewport during editing and navigation. Large documents can make
these repeated scans expensive, particularly on embedded hardware. Improve
editing and navigation responsiveness while preserving the supported byte
navigation, wrapping, cursor visibility, and frame behavior.

The current application has one window with fixed dimensions. Resizing and
multiple windows are future capabilities, not current supported behavior.
If those capabilities are included, windows showing the same document should
reuse layout work where their widths allow it while retaining independent
cursor and viewport state.

## Scope and constraints

- Establish representative large-file editing and navigation workloads and
  measurable performance and memory targets before accepting an approach.
  No latency target or fixed memory ceiling has been agreed yet.
- Decide whether the first increment improves only the current single-window
  editor or also introduces resize and multiple-window support. Keep deferred
  capabilities explicit rather than assuming they exist.
- Preserve the current editor's byte-based wrapping and navigation behavior,
  gutter handling, owned frame snapshots, and cursor visibility guarantees.
- Keep presentation layout private to the editor. The document model supplies
  bytes, logical lines, and wrapped-row views without depending on viewport or
  presentation state. Sharing work per document must preserve that direction.
- Identify shared documents by document identity rather than a filename alone.
- Account for memory as document size and the number of distinct active widths
  grow. Retaining historical widths after resize or window closure must have a
  bound; this does not by itself establish a fixed byte ceiling.

## Acceptance considerations

These are draft outcomes to refine into acceptance scenarios and measurements
with the implementation plan. They are not yet persisted `.feature` scenarios
or evidence that this capability exists.

- Editing and navigation meet the agreed targets on representative large files.
  Repeated navigation and editing between lookups must both be measured; a
  faster repeated lookup alone does not establish responsive editing.
- Wrapped-row counts and frame output remain correct at line boundaries and
  EOF, and edits never cause a later frame to use stale layout information.
- Successful edits, no-op edits, and failed edits leave subsequent layout use
  consistent with the actual document contents; a failed update must not expose
  partially rebuilt layout as valid.
- If resize is included, changing width produces the correct wrapped view and
  cursor visibility. Revisiting a width remains correct even if retained layout
  work has been discarded.
- If multiple windows are included, same-width windows can share layout work
  while their cursors and viewports remain independent. Different-width windows
  remain correct without repeatedly discarding layout work still in active use.
- Memory measurements cover large line counts, distinct active widths, and
  resize history. Acceptance must distinguish active memory growth from the
  bound on inactive retained work.

## Open questions

Resolve the workload, performance targets, memory budget, and first-increment
scope before considering this spec agreed. The implementation plan must also
resolve layout ownership, edit-update strategy, layout/view lifetimes, and
whether retaining inactive widths justifies its complexity. A full rebuild on
every editing keystroke may still miss the desired outcome even if repeated
navigation becomes faster.

Retain this spec as intent and rationale when implemented. Record final scope
and link the accepted scenarios here, and update the component README with the
supported contract. Later changes should identify any superseded requirements.
