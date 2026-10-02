#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "document.h"
#include "document_view.h"
#include "gap_loader.h"
#include "key.h"
#include "raw_mode.h"
#include "read_key.h"
#include "terminal.h"
#include "wrapped_row_range.h"

namespace {

constexpr size_t kLineNumberIndent = 4;

size_t TextWidth(size_t total_width) {
  const size_t min_total_width = kLineNumberIndent + 1;
  const size_t safe_total_width = std::max(total_width, min_total_width);
  return safe_total_width - kLineNumberIndent;
}

struct Window {
  Document* doc{};
  size_t width{80};
  size_t height{25};
  size_t top_row{0};
  ByteIndex cursor_pos{0};
  std::optional<size_t> preferred_column{};
};

struct RenderCache {
  std::vector<std::string> last_drawn_rows{};
  bool has_last_drawn_rows = false;
};

struct FrameRow {
  Document::LineNumber line_number = 0;
  ByteIndex start = 0;
  ByteIndex end = 0;
  bool show_line_number = false;
  bool is_last_wrapped_row_of_line = false;
};

struct Frame {
  std::vector<FrameRow> rows;
  size_t text_width = 1;
  size_t height = 0;
};

struct ScreenCursor {
  size_t row = 1;
  size_t col = 0;
  bool is_visible = false;
};

Frame BuildFrame(const Window& window);
ScreenCursor LocateCursorInFrame(const Frame& frame, ByteIndex cursor_pos);

bool CursorBelongsToRow(ByteIndex cursor_pos, const FrameRow& row) {
  return cursor_pos < row.end ||
         (cursor_pos == row.end && row.is_last_wrapped_row_of_line);
}

size_t WrappedRowCountForLine(const Window& window,
                              Document::LineNumber line_number) {
  auto lines = window.doc->LinesFrom(line_number);
  if (lines.empty()) {
    return 0;
  }

  WrappedRowRange rows(*window.doc, *lines.begin(),
                       static_cast<ByteCount>(TextWidth(window.width)));
  return rows.row_count();
}

size_t TotalWrappedRowCount(const Window& window) {
  const auto num_lines =
      static_cast<Document::LineNumber>(window.doc->NumLines());
  if (num_lines == 0) {
    return 0;
  }

  size_t total = 0;
  for (Document::LineNumber line = 0; line < num_lines; ++line) {
    total += WrappedRowCountForLine(window, line);
  }
  return total;
}

void NormalizeTopAnchor(Window& window) {
  const size_t total_rows = TotalWrappedRowCount(window);
  if (total_rows == 0) {
    window.top_row = 0;
    return;
  }

  if (window.top_row >= total_rows) {
    window.top_row = total_rows - 1;
  }
}

void ScrollDownOneRow(Window& window) {
  const auto num_lines =
      static_cast<Document::LineNumber>(window.doc->NumLines());
  if (num_lines == 0) {
    return;
  }

  NormalizeTopAnchor(window);
  const size_t total_rows = TotalWrappedRowCount(window);
  if (window.top_row + 1 < total_rows) {
    ++window.top_row;
  }
}

void ScrollUpOneRow(Window& window) {
  const auto num_lines =
      static_cast<Document::LineNumber>(window.doc->NumLines());
  if (num_lines == 0) {
    return;
  }

  NormalizeTopAnchor(window);
  if (window.top_row > 0) {
    --window.top_row;
  }
}

void MoveCursorToTopVisibleRow(Window& window) {
  const Frame frame = BuildFrame(window);
  if (!frame.rows.empty()) {
    window.cursor_pos = frame.rows.front().start;
  }
}

void ScrollPageUp(Window& window) {
  const size_t page_rows = window.height > 1 ? window.height - 1 : 1;
  for (size_t i = 0; i < page_rows; ++i) {
    ScrollUpOneRow(window);
  }
  MoveCursorToTopVisibleRow(window);
}

void ScrollPageDown(Window& window) {
  const size_t page_rows = window.height > 1 ? window.height - 1 : 1;
  for (size_t i = 0; i < page_rows; ++i) {
    ScrollDownOneRow(window);
  }
  MoveCursorToTopVisibleRow(window);
}

void MoveToDocumentStart(Window& window) {
  window.cursor_pos = 0;
  window.top_row = 0;
  window.preferred_column.reset();
}

void MoveToDocumentEnd(Window& window) {
  window.cursor_pos = window.doc->Len();

  const auto num_lines =
      static_cast<Document::LineNumber>(window.doc->NumLines());
  if (num_lines == 0) {
    window.top_row = 0;
    return;
  }

  const size_t total_rows = TotalWrappedRowCount(window);
  window.top_row = total_rows > window.height ? total_rows - window.height : 0;
  window.preferred_column.reset();
}

ByteIndex LineEndExcludingNewline(const Document& doc,
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

void MoveToLineStart(Window& window) {
  if (window.doc->NumLines() == 0) {
    window.cursor_pos = 0;
    return;
  }

  const Document::LineNumber line = window.doc->LineFromPos(window.cursor_pos);
  window.cursor_pos = window.doc->StartOfLine(line);
  window.preferred_column.reset();
}

void MoveToLineEnd(Window& window) {
  if (window.doc->NumLines() == 0) {
    window.cursor_pos = 0;
    return;
  }

  const Document::LineNumber line = window.doc->LineFromPos(window.cursor_pos);
  window.cursor_pos = LineEndExcludingNewline(*window.doc, line);
  window.preferred_column.reset();
}

void MoveCursorVertical(Window& window, int delta) {
  if (window.doc->NumLines() == 0) {
    window.cursor_pos = 0;
    return;
  }

  const Frame frame = BuildFrame(window);
  if (frame.rows.empty()) {
    return;
  }

  const ScreenCursor cursor = LocateCursorInFrame(frame, window.cursor_pos);
  if (!cursor.is_visible) {
    return;
  }

  const int current_index = static_cast<int>(cursor.row) - 1;
  const int target_index = current_index + delta;
  if (target_index < 0 ||
      target_index >= static_cast<int>(frame.rows.size())) {
    return;
  }

  const auto& current_row = frame.rows[static_cast<size_t>(current_index)];
  const auto& target_row = frame.rows[static_cast<size_t>(target_index)];

  const size_t current_col =
      static_cast<size_t>(window.cursor_pos - current_row.start);
  const size_t current_width =
      static_cast<size_t>(current_row.end - current_row.start);
  const size_t desired_col =
      window.preferred_column.value_or(std::min(current_col, current_width));
  const size_t target_width =
      static_cast<size_t>(target_row.end - target_row.start);
  const size_t target_col = target_width == 0 ? desired_col : std::min(desired_col, target_width);

  window.cursor_pos =
      target_row.start + static_cast<ByteIndex>(std::min(target_col, target_width));
  window.preferred_column = desired_col;
}

void MoveCursorVerticalWithBoundaryScroll(Window& window, int delta) {
  const Frame before_frame = BuildFrame(window);
  const ScreenCursor before_cursor =
      LocateCursorInFrame(before_frame, window.cursor_pos);

  if (!before_cursor.is_visible) {
    return;
  }

  if (delta < 0 && before_cursor.row == 1) {
    ScrollUpOneRow(window);
    const Frame after_frame = BuildFrame(window);
    if (!after_frame.rows.empty()) {
      window.cursor_pos = after_frame.rows.front().start;
    }
    return;
  }

  if (delta > 0 && before_cursor.row == window.height) {
    ScrollDownOneRow(window);
    const Frame after_frame = BuildFrame(window);
    if (!after_frame.rows.empty()) {
      const auto& last_row = after_frame.rows.back();
      window.cursor_pos = last_row.start;
    }
    return;
  }

  const ByteIndex old_pos = window.cursor_pos;
  MoveCursorVertical(window, delta);
  if (window.cursor_pos == old_pos) {
    return;
  }
}

void MoveCursorLeft(Window& window) {
  if (window.cursor_pos > 0) {
    --window.cursor_pos;
  }
  window.preferred_column.reset();
}

void MoveCursorRight(Window& window) {
  if (window.cursor_pos < window.doc->Len()) {
    ++window.cursor_pos;
  }
  window.preferred_column.reset();
}

std::pair<Document::LineNumber, size_t> TopAnchorForRow(const Window& window,
                                                        size_t top_row) {
  const auto num_lines =
      static_cast<Document::LineNumber>(window.doc->NumLines());
  if (num_lines == 0) {
    return {0, 0};
  }

  const size_t total_rows = TotalWrappedRowCount(window);
  if (total_rows == 0) {
    return {0, 0};
  }

  size_t remaining = std::min(top_row, total_rows - 1);
  for (Document::LineNumber line = 0; line < num_lines; ++line) {
    const size_t row_count = WrappedRowCountForLine(window, line);
    if (remaining < row_count) {
      return {line, remaining};
    }
    remaining -= row_count;
  }

  return {num_lines - 1, 0};
}

Frame BuildFrame(const Window& window) {
  const size_t text_width = TextWidth(window.width);
  Frame frame{.rows = {}, .text_width = text_width, .height = window.height};
  frame.rows.reserve(window.height);

  const auto [top_line, top_row_offset] = TopAnchorForRow(window, window.top_row);
  auto lines = window.doc->LinesFrom(top_line);
  bool is_first_line = true;
  for (auto line_it = lines.begin();
       line_it != lines.end() && frame.rows.size() < window.height; ++line_it) {
    const auto line_number = line_it.line_number();
    WrappedRowRange rows(*window.doc, *line_it,
                         static_cast<ByteCount>(text_width));

    bool show_line_number = is_first_line ? (top_row_offset == 0) : true;
    for (auto row_it = rows.begin();
         row_it != rows.end() && frame.rows.size() < window.height; ++row_it) {
      if (is_first_line && row_it.row_index() < top_row_offset) {
        continue;
      }

      const auto row_view = *row_it;
      frame.rows.push_back(FrameRow{
          .line_number = line_number,
          .start = row_view.StartIndex(),
          .end = row_view.EndIndex(),
          .show_line_number = show_line_number,
          .is_last_wrapped_row_of_line =
              row_it.row_index() + 1 == rows.row_count(),
      });
      show_line_number = false;
    }

    is_first_line = false;
  }

  return frame;
}

ScreenCursor LocateCursorInFrame(const Frame& frame, ByteIndex cursor_pos) {
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

Document LoadDocument(const char* filename) {
  std::vector<uint8_t> data = gap_loader::Load(filename);
  return Document{std::move(data)};
}

Frame KeepCursorVisible(Window& window, Frame frame) {
  const auto num_lines =
      static_cast<Document::LineNumber>(window.doc->NumLines());
  if (num_lines == 0) {
    window.top_row = 0;
    return frame;
  }

  NormalizeTopAnchor(window);
  const size_t total_rows = TotalWrappedRowCount(window);
  if (total_rows == 0) {
    return frame;
  }

  size_t cursor_row = 0;
  for (Document::LineNumber line = 0; line < num_lines; ++line) {
    const auto lines = window.doc->LinesFrom(line);
    if (lines.empty()) {
      continue;
    }

    const WrappedRowRange rows(*window.doc, *lines.begin(),
                              static_cast<ByteCount>(TextWidth(window.width)));
    const bool is_last_line = line + 1 == num_lines;
    const ByteIndex line_end = line + 1 < num_lines
                                  ? window.doc->StartOfLine(line + 1)
                                  : window.doc->Len();

    if (window.cursor_pos < line_end ||
        (is_last_line && window.cursor_pos == line_end)) {
      for (auto row_it = rows.begin(); row_it != rows.end(); ++row_it) {
        const auto row_view = *row_it;
        if (window.cursor_pos < row_view.EndIndex() ||
            (window.cursor_pos == row_view.EndIndex() &&
             row_it.row_index() + 1 == rows.row_count())) {
          cursor_row += row_it.row_index();
          break;
        }
      }
      break;
    }

    cursor_row += rows.row_count();
  }

  const size_t first_visible = window.top_row;
  const size_t last_visible =
      std::min(total_rows - 1, first_visible + frame.rows.size() - 1);

  if (cursor_row < first_visible) {
    window.top_row = cursor_row;
  } else if (cursor_row > last_visible) {
    window.top_row = std::min(cursor_row - (frame.rows.size() - 1), total_rows - 1);
  }

  return BuildFrame(window);
}

std::pair<size_t, size_t> CursorScreenPosition(const Frame& frame,
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

std::string BuildRowText(const Window& window, const Frame& frame, size_t row) {
  if (row >= frame.rows.size()) {
    return "~";
  }

  const auto& frame_row = frame.rows[row];
  std::string out = frame_row.show_line_number
                        ? std::format("{:3} ", frame_row.line_number + 1)
                        : "    ";

  const ByteCount count = frame_row.end - frame_row.start;
  for (Byte b : window.doc->PartialView(frame_row.start, count)) {
    out.push_back(static_cast<char>(b));
  }

  return out;
}

void Draw(const Window& window, const Frame& frame, RenderCache& cache) {
  std::vector<std::string> next_rows;
  next_rows.reserve(window.height);

  for (size_t row = 0; row < window.height; ++row) {
    next_rows.push_back(BuildRowText(window, frame, row));

    if (!cache.has_last_drawn_rows || row >= cache.last_drawn_rows.size() ||
        next_rows[row] != cache.last_drawn_rows[row]) {
      terminal::MoveTo(row + 1, 1);
      terminal::PutString(next_rows[row]);
      terminal::ClearToEol();
    }
  }

  cache.last_drawn_rows = std::move(next_rows);
  cache.has_last_drawn_rows = true;

  const auto [row, col] = CursorScreenPosition(frame, window.cursor_pos);
  terminal::MoveTo(row, col);
  terminal::Flush();
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>\n";
    return 1;
  }

  Document doc = LoadDocument(argv[1]);

  Window window{
      .doc = &doc,
      .width = 80,
      .height = 25,
      .top_row = 0,
      .cursor_pos = 0,
  };
  RenderCache render_cache{};

  Frame frame = BuildFrame(window);
  frame = KeepCursorVisible(window, frame);
  EnableRawMode();
  terminal::Clear();
  Draw(window, frame, render_cache);

  while (true) {
    std::optional<Key> key = ReadKey();
    if (!key) {
      if (!isatty(STDIN_FILENO)) {
        break;
      }
      continue;
    }

    if (key->Is(EditingKey::Escape, KeyMods::Alt)) {
      break;
    }

    if (key->IsText()) {
      const std::string text = key->Text();
      doc.Edit(window.cursor_pos, 0, AsByteSpan(text));
      window.cursor_pos += static_cast<ByteIndex>(text.size());
      window.preferred_column.reset();
    } else if (key->Is(EditingKey::Backspace)) {
      if (window.cursor_pos > 0) {
        doc.Edit(window.cursor_pos - 1, 1, {});
        --window.cursor_pos;
      }
      window.preferred_column.reset();
    } else if (key->Is(EditingKey::Delete)) {
      if (window.cursor_pos < doc.Len()) {
        doc.Edit(window.cursor_pos, 1, {});
      }
      window.preferred_column.reset();
    } else if (key->Is(EditingKey::Enter)) {
      doc.Edit(window.cursor_pos, 0, AsByteSpan("\n"));
      ++window.cursor_pos;
      window.preferred_column.reset();
    } else if (key->Is(NavigationKey::Up)) {
      MoveCursorVerticalWithBoundaryScroll(window, -1);
    } else if (key->Is(NavigationKey::Down)) {
      MoveCursorVerticalWithBoundaryScroll(window, 1);
    } else if (key->Is(NavigationKey::Left)) {
      MoveCursorLeft(window);
    } else if (key->Is(NavigationKey::Right)) {
      MoveCursorRight(window);
    } else if (key->Is(NavigationKey::PageUp)) {
      ScrollPageUp(window);
      window.preferred_column.reset();
    } else if (key->Is(NavigationKey::PageDown)) {
      ScrollPageDown(window);
      window.preferred_column.reset();
    } else if (key->Is(NavigationKey::Home, KeyMods::Ctrl)) {
      MoveToDocumentStart(window);
    } else if (key->Is(NavigationKey::End, KeyMods::Ctrl)) {
      MoveToDocumentEnd(window);
    } else if (key->Is(NavigationKey::Home)) {
      MoveToLineStart(window);
    } else if (key->Is(NavigationKey::End)) {
      MoveToLineEnd(window);
    }

    Frame frame = BuildFrame(window);
    frame = KeepCursorVisible(window, frame);
    Draw(window, frame, render_cache);
  }

  terminal::Clear();
  return 0;
}
