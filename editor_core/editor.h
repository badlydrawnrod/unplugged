#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "document.h"
#include "key.h"

namespace unplugged {

// An owned snapshot: rows include line-number gutters and filler rows. Cursor
// coordinates are one-based terminal cells, with the column clamped to the last
// cell of the text area. Wrapping and navigation currently count bytes.
struct EditorFrame {
  std::vector<std::string> rows;
  size_t text_width = 1;
  size_t cursor_row = 1;
  size_t cursor_column = 5;
};

// Owns one document and its viewport state. It performs no terminal or file
// I/O.
class Editor {
 public:
  // Widths below five are treated as a four-cell gutter plus one text cell.
  // Throws invalid_argument for zero height or an unrepresentable text width.
  explicit Editor(Document document, size_t width = 80, size_t height = 25);

  // Returns false for Alt+Escape (exit), true otherwise. Unknown keys are
  // no-ops. Navigation uses bytes, including movement through newline and UTF-8
  // bytes. At a viewport boundary, vertical movement selects the new row's
  // start.
  bool HandleKey(const Key& key);

  // Returns a snapshot independent of future edits. Commands keep the cursor
  // visible before returning. Snapshot creation does not change editor state.
  EditorFrame CreateFrame() const;
  const Document& GetDocument() const { return document_; }
  ByteIndex CursorPosition() const { return cursor_pos_; }
  size_t TopRow() const { return top_row_; }

 private:
  enum class VerticalDirection { Up, Down };

  struct FrameRow {
    Document::LineNumber line_number = 0;
    ByteIndex start = 0;
    ByteIndex end = 0;
    bool show_line_number = false;
    bool is_last_wrapped_row_of_line = false;
  };
  struct LayoutFrame {
    std::vector<FrameRow> rows;
    size_t text_width = 1;
    size_t height = 0;
  };
  struct ScreenCursor {
    size_t row = 1;
    size_t col = 0;
    bool is_visible = false;
  };

  static bool CursorBelongsToRow(ByteIndex cursor_pos, const FrameRow& row);
  static ByteIndex LineEndExcludingNewline(const Document& doc,
                                           Document::LineNumber line_number);
  static ScreenCursor LocateCursorInFrame(const LayoutFrame& frame,
                                          ByteIndex cursor_pos);
  static std::pair<size_t, size_t> CursorScreenPosition(
      const LayoutFrame& frame, ByteIndex cursor_pos);
  size_t WrappedRowCountForLine(Document::LineNumber line_number) const;
  size_t TotalWrappedRowCount() const;
  std::pair<Document::LineNumber, size_t> TopAnchorForRow(size_t top_row) const;
  LayoutFrame BuildFrame() const;
  void KeepCursorVisible(const LayoutFrame& frame);
  void NormalizeTopAnchor();
  void ScrollDownOneRow();
  void ScrollUpOneRow();
  void MoveCursorToTopVisibleRow();
  void ScrollPageUp();
  void ScrollPageDown();
  void MoveToDocumentStart();
  void MoveToDocumentEnd();
  void MoveToLineStart();
  void MoveToLineEnd();
  void MoveCursorVertical(VerticalDirection direction);
  void MoveCursorVerticalWithBoundaryScroll(VerticalDirection direction);
  void MoveCursorLeft();
  void MoveCursorRight();

  Document document_;
  size_t width_;
  size_t height_;
  size_t top_row_ = 0;
  ByteIndex cursor_pos_ = 0;
  std::optional<size_t> preferred_column_;
};

}  // namespace unplugged
