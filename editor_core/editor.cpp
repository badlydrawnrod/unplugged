#include "editor_core/editor.h"

#include <algorithm>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

#include "document/document_view.h"
#include "document/wrapped_row_range.h"

namespace unplugged {
namespace {
constexpr size_t kLineNumberIndent = 4;

size_t TextWidth(size_t total_width) {
  return std::max(total_width, kLineNumberIndent + 1) - kLineNumberIndent;
}
}  // namespace

Editor::Editor(Document document, size_t width, size_t height)
    : document_(std::move(document)), width_(width), height_(height) {
  if (height == 0 || TextWidth(width) > std::numeric_limits<ByteCount>::max()) {
    throw std::invalid_argument("Unsupported editor viewport dimensions");
  }
}

bool Editor::CursorBelongsToRow(ByteIndex cursor_pos, const FrameRow& row) {
  return cursor_pos < row.end ||
         (cursor_pos == row.end && row.is_last_wrapped_row_of_line);
}

size_t Editor::WrappedRowCountForLine(Document::LineNumber line_number) const {
  auto lines = document_.LinesFrom(line_number);
  if (lines.empty()) {
    return 0;
  }

  WrappedRowRange rows(document_, *lines.begin(),
                       static_cast<ByteCount>(TextWidth(width_)));
  return rows.NumRows();
}

size_t Editor::TotalWrappedRowCount() const {
  const auto num_lines =
      static_cast<Document::LineNumber>(document_.NumLines());
  if (num_lines == 0) {
    return 0;
  }

  size_t total = 0;
  for (Document::LineNumber line = 0; line < num_lines; ++line) {
    total += WrappedRowCountForLine(line);
  }
  return total;
}

void Editor::NormalizeTopAnchor() {
  const size_t total_rows = TotalWrappedRowCount();
  if (total_rows == 0) {
    top_row_ = 0;
    return;
  }

  if (top_row_ >= total_rows) {
    top_row_ = total_rows - 1;
  }
}

void Editor::ScrollDownOneRow() {
  const auto num_lines =
      static_cast<Document::LineNumber>(document_.NumLines());
  if (num_lines == 0) {
    return;
  }

  NormalizeTopAnchor();
  const size_t total_rows = TotalWrappedRowCount();
  if (top_row_ + 1 < total_rows) {
    ++top_row_;
  }
}

void Editor::ScrollUpOneRow() {
  const auto num_lines =
      static_cast<Document::LineNumber>(document_.NumLines());
  if (num_lines == 0) {
    return;
  }

  NormalizeTopAnchor();
  if (top_row_ > 0) {
    --top_row_;
  }
}

void Editor::MoveCursorToTopVisibleRow() {
  const LayoutFrame frame = BuildFrame();
  if (!frame.rows.empty()) {
    cursor_pos_ = frame.rows.front().start;
  }
}

void Editor::ScrollPageUp() {
  const size_t page_rows = height_ > 1 ? height_ - 1 : 1;
  for (size_t i = 0; i < page_rows; ++i) {
    ScrollUpOneRow();
  }
  MoveCursorToTopVisibleRow();
}

void Editor::ScrollPageDown() {
  const size_t page_rows = height_ > 1 ? height_ - 1 : 1;
  for (size_t i = 0; i < page_rows; ++i) {
    ScrollDownOneRow();
  }
  MoveCursorToTopVisibleRow();
}

void Editor::MoveToDocumentStart() {
  cursor_pos_ = 0;
  top_row_ = 0;
  preferred_column_.reset();
}

void Editor::MoveToDocumentEnd() {
  cursor_pos_ = document_.Len();

  const auto num_lines =
      static_cast<Document::LineNumber>(document_.NumLines());
  if (num_lines == 0) {
    top_row_ = 0;
    return;
  }

  const size_t total_rows = TotalWrappedRowCount();
  top_row_ = total_rows > height_ ? total_rows - height_ : 0;
  preferred_column_.reset();
}

ByteIndex Editor::LineEndExcludingNewline(const Document& doc,
                                          Document::LineNumber line_number) {
  const ByteIndex line_start = doc.StartOfLine(line_number);
  const ByteIndex line_end = line_number + 1 < doc.NumLines()
                                 ? doc.StartOfLine(line_number + 1)
                                 : doc.Len();

  if (line_end > line_start && doc.At(line_end - 1) == '\n') {
    return line_end - 1;
  }

  return line_end;
}

void Editor::MoveToLineStart() {
  if (document_.NumLines() == 0) {
    cursor_pos_ = 0;
    return;
  }

  const Document::LineNumber line = document_.LineFromPos(cursor_pos_);
  cursor_pos_ = document_.StartOfLine(line);
  preferred_column_.reset();
}

void Editor::MoveToLineEnd() {
  if (document_.NumLines() == 0) {
    cursor_pos_ = 0;
    return;
  }

  const Document::LineNumber line = document_.LineFromPos(cursor_pos_);
  cursor_pos_ = LineEndExcludingNewline(document_, line);
  preferred_column_.reset();
}

void Editor::MoveCursorVertical(VerticalDirection direction) {
  if (document_.NumLines() == 0) {
    cursor_pos_ = 0;
    return;
  }

  const LayoutFrame frame = BuildFrame();
  if (frame.rows.empty()) {
    return;
  }

  const ScreenCursor cursor = LocateCursorInFrame(frame, cursor_pos_);
  if (!cursor.is_visible) {
    return;
  }

  const size_t current_index = cursor.row - 1;
  if ((direction == VerticalDirection::Up && current_index == 0) ||
      (direction == VerticalDirection::Down &&
       current_index + 1 >= frame.rows.size())) {
    return;
  }
  const size_t target_index = direction == VerticalDirection::Up
                                  ? current_index - 1
                                  : current_index + 1;
  const auto& current_row = frame.rows[current_index];
  const auto& target_row = frame.rows[target_index];

  const size_t current_col =
      static_cast<size_t>(cursor_pos_ - current_row.start);
  const size_t current_width =
      static_cast<size_t>(current_row.end - current_row.start);
  const size_t desired_col =
      preferred_column_.value_or(std::min(current_col, current_width));
  const size_t target_width =
      static_cast<size_t>(target_row.end - target_row.start);
  const size_t target_col =
      target_width == 0 ? desired_col : std::min(desired_col, target_width);

  cursor_pos_ = target_row.start +
                static_cast<ByteIndex>(std::min(target_col, target_width));
  preferred_column_ = desired_col;
}

void Editor::MoveCursorVerticalWithBoundaryScroll(VerticalDirection direction) {
  const LayoutFrame before_frame = BuildFrame();
  const ScreenCursor before_cursor =
      LocateCursorInFrame(before_frame, cursor_pos_);

  if (!before_cursor.is_visible) {
    return;
  }

  if (direction == VerticalDirection::Up && before_cursor.row == 1) {
    ScrollUpOneRow();
    const LayoutFrame after_frame = BuildFrame();
    if (!after_frame.rows.empty()) {
      cursor_pos_ = after_frame.rows.front().start;
    }
    return;
  }

  if (direction == VerticalDirection::Down && before_cursor.row == height_) {
    ScrollDownOneRow();
    const LayoutFrame after_frame = BuildFrame();
    if (!after_frame.rows.empty()) {
      const auto& last_row = after_frame.rows.back();
      cursor_pos_ = last_row.start;
    }
    return;
  }

  const ByteIndex old_pos = cursor_pos_;
  MoveCursorVertical(direction);
  if (cursor_pos_ == old_pos) {
    return;
  }
}

void Editor::MoveCursorLeft() {
  if (cursor_pos_ > 0) {
    --cursor_pos_;
  }
  preferred_column_.reset();
}

void Editor::MoveCursorRight() {
  if (cursor_pos_ < document_.Len()) {
    ++cursor_pos_;
  }
  preferred_column_.reset();
}

std::pair<Document::LineNumber, size_t> Editor::TopAnchorForRow(
    size_t top_row) const {
  const auto num_lines =
      static_cast<Document::LineNumber>(document_.NumLines());
  if (num_lines == 0) {
    return {0, 0};
  }

  const size_t total_rows = TotalWrappedRowCount();
  if (total_rows == 0) {
    return {0, 0};
  }

  size_t remaining = std::min(top_row, total_rows - 1);
  for (Document::LineNumber line = 0; line < num_lines; ++line) {
    const size_t row_count = WrappedRowCountForLine(line);
    if (remaining < row_count) {
      return {line, remaining};
    }
    remaining -= row_count;
  }

  return {num_lines - 1, 0};
}

Editor::LayoutFrame Editor::BuildFrame() const {
  const size_t text_width = TextWidth(width_);
  LayoutFrame frame{.rows = {}, .text_width = text_width, .height = height_};
  frame.rows.reserve(height_);

  const auto [top_line, top_row_offset] = TopAnchorForRow(top_row_);
  auto lines = document_.LinesFrom(top_line);
  bool is_first_line = true;
  for (auto line_it = lines.begin();
       line_it != lines.end() && frame.rows.size() < height_; ++line_it) {
    const auto line_number = line_it.GetLineNumber();
    WrappedRowRange rows(document_, *line_it,
                         static_cast<ByteCount>(text_width));

    bool show_line_number = is_first_line ? (top_row_offset == 0) : true;
    for (auto row_it = rows.begin();
         row_it != rows.end() && frame.rows.size() < height_; ++row_it) {
      if (is_first_line && row_it.GetRowIndex() < top_row_offset) {
        continue;
      }

      const auto row_view = *row_it;
      frame.rows.push_back(FrameRow{
          .line_number = line_number,
          .start = row_view.StartIndex(),
          .end = row_view.EndIndex(),
          .show_line_number = show_line_number,
          .is_last_wrapped_row_of_line =
              row_it.GetRowIndex() + 1 == rows.NumRows(),
      });
      show_line_number = false;
    }

    is_first_line = false;
  }

  return frame;
}

Editor::ScreenCursor Editor::LocateCursorInFrame(const LayoutFrame& frame,
                                                 ByteIndex cursor_pos) {
  for (size_t i = 0; i < frame.rows.size(); ++i) {
    const auto& row = frame.rows[i];
    if (CursorBelongsToRow(cursor_pos, row)) {
      size_t col = static_cast<size_t>(cursor_pos - row.start);
      const size_t row_width = static_cast<size_t>(row.end - row.start);
      if (col > row_width) {
        col = row_width;
      }
      return {.row = i + 1, .col = col, .is_visible = true};
    }
  }

  return {.row = frame.height, .col = 0, .is_visible = false};
}

void Editor::KeepCursorVisible(const LayoutFrame& frame) {
  const auto num_lines =
      static_cast<Document::LineNumber>(document_.NumLines());
  if (num_lines == 0) {
    top_row_ = 0;
    return;
  }

  NormalizeTopAnchor();
  const size_t total_rows = TotalWrappedRowCount();
  if (total_rows == 0) {
    return;
  }

  size_t cursor_row = 0;
  for (Document::LineNumber line = 0; line < num_lines; ++line) {
    const auto lines = document_.LinesFrom(line);
    if (lines.empty()) {
      continue;
    }

    const WrappedRowRange rows(document_, *lines.begin(),
                               static_cast<ByteCount>(TextWidth(width_)));
    const bool is_last_line = line + 1 == num_lines;
    const ByteIndex line_end = line + 1 < num_lines
                                   ? document_.StartOfLine(line + 1)
                                   : document_.Len();

    if (cursor_pos_ < line_end || (is_last_line && cursor_pos_ == line_end)) {
      for (auto row_it = rows.begin(); row_it != rows.end(); ++row_it) {
        const auto row_view = *row_it;
        if (cursor_pos_ < row_view.EndIndex() ||
            (cursor_pos_ == row_view.EndIndex() &&
             row_it.GetRowIndex() + 1 == rows.NumRows())) {
          cursor_row += row_it.GetRowIndex();
          break;
        }
      }
      break;
    }

    cursor_row += rows.NumRows();
  }

  const size_t first_visible = top_row_;
  const size_t last_visible =
      std::min(total_rows - 1, first_visible + frame.rows.size() - 1);

  if (cursor_row < first_visible) {
    top_row_ = cursor_row;
  } else if (cursor_row > last_visible) {
    top_row_ = std::min(cursor_row - (frame.rows.size() - 1), total_rows - 1);
  }
}

std::pair<size_t, size_t> Editor::CursorScreenPosition(const LayoutFrame& frame,
                                                       ByteIndex cursor_pos) {
  if (frame.rows.empty()) {
    return {1, 1 + kLineNumberIndent};
  }

  const ScreenCursor cursor = LocateCursorInFrame(frame, cursor_pos);
  const size_t row = cursor.is_visible ? cursor.row : frame.height;
  // Cursor columns are zero-based in the text area, so clamp to the last
  // visible cell to avoid positioning at terminal column (width + 1).
  const size_t col = std::min(frame.text_width - 1, cursor.col);
  return {row, col + 1 + kLineNumberIndent};
}

bool Editor::HandleKey(const Key& key) {
  if (key.Is(EditingKey::Escape, KeyMods::Alt)) {
    return false;
  }
  if (key.IsText()) {
    const std::string text = key.Text();
    document_.Edit(cursor_pos_, 0, AsByteSpan(text));
    cursor_pos_ += static_cast<ByteIndex>(text.size());
    preferred_column_.reset();
  } else if (key.Is(EditingKey::Backspace)) {
    if (cursor_pos_ > 0) {
      document_.Edit(cursor_pos_ - 1, 1, {});
      --cursor_pos_;
    }
    preferred_column_.reset();
  } else if (key.Is(EditingKey::Delete)) {
    if (cursor_pos_ < document_.Len()) {
      document_.Edit(cursor_pos_, 1, {});
    }
    preferred_column_.reset();
  } else if (key.Is(EditingKey::Enter)) {
    document_.Edit(cursor_pos_, 0, AsByteSpan("\n"));
    ++cursor_pos_;
    preferred_column_.reset();
  } else if (key.Is(NavigationKey::Up)) {
    MoveCursorVerticalWithBoundaryScroll(VerticalDirection::Up);
  } else if (key.Is(NavigationKey::Down)) {
    MoveCursorVerticalWithBoundaryScroll(VerticalDirection::Down);
  } else if (key.Is(NavigationKey::Left)) {
    MoveCursorLeft();
  } else if (key.Is(NavigationKey::Right)) {
    MoveCursorRight();
  } else if (key.Is(NavigationKey::PageUp)) {
    ScrollPageUp();
    preferred_column_.reset();
  } else if (key.Is(NavigationKey::PageDown)) {
    ScrollPageDown();
    preferred_column_.reset();
  } else if (key.Is(NavigationKey::Home, KeyMods::Ctrl)) {
    MoveToDocumentStart();
  } else if (key.Is(NavigationKey::End, KeyMods::Ctrl)) {
    MoveToDocumentEnd();
  } else if (key.Is(NavigationKey::Home)) {
    MoveToLineStart();
  } else if (key.Is(NavigationKey::End)) {
    MoveToLineEnd();
  }
  KeepCursorVisible(BuildFrame());
  return true;
}

EditorFrame Editor::CreateFrame() const {
  const LayoutFrame layout = BuildFrame();
  EditorFrame frame;
  frame.text_width = layout.text_width;
  frame.rows.reserve(height_);
  for (size_t row = 0; row < height_; ++row) {
    if (row >= layout.rows.size()) {
      frame.rows.emplace_back("~");
      continue;
    }
    const auto& frame_row = layout.rows[row];
    std::string text = frame_row.show_line_number
                           ? std::format("{:3} ", frame_row.line_number + 1)
                           : "    ";
    for (Byte byte : document_.PartialView(frame_row.start,
                                           frame_row.end - frame_row.start)) {
      text.push_back(static_cast<char>(byte));
    }
    frame.rows.push_back(std::move(text));
  }
  const auto [row, column] = CursorScreenPosition(layout, cursor_pos_);
  frame.cursor_row = row;
  frame.cursor_column = column;
  return frame;
}

}  // namespace unplugged
