#include "editor_core/editor.h"

#include <unistd.h>

#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "document/document.h"
#include "file_loader/load.h"
#include "input_protocol.h"
#include "raw_mode.h"
#include "read_key.h"
#include "terminal.h"

namespace {

struct RenderCache {
  std::vector<std::string> last_drawn_rows{};
  bool has_last_drawn_rows = false;
};

void Draw(terminal::Output& output, const unplugged::EditorFrame& frame,
          RenderCache& cache) {
  std::vector<std::string> next_rows;
  next_rows.reserve(frame.rows.size());

  for (size_t row = 0; row < frame.rows.size(); ++row) {
    next_rows.push_back(frame.rows[row]);

    if (!cache.has_last_drawn_rows || row >= cache.last_drawn_rows.size() ||
        next_rows[row] != cache.last_drawn_rows[row]) {
      output.MoveTo(row + 1, 1);
      output.PutString(next_rows[row]);
      output.ClearToEol();
    }
  }

  cache.last_drawn_rows = std::move(next_rows);
  cache.has_last_drawn_rows = true;

  output.MoveTo(frame.cursor_row, frame.cursor_column);
  output.Flush();
}

void Report(std::string_view prefix, std::string_view detail,
            std::string_view suffix) noexcept {
  try {
    terminal::Output diagnostic(STDERR_FILENO);
    diagnostic.PutString(prefix);
    diagnostic.PutString(detail);
    diagnostic.PutString(suffix);
    diagnostic.Flush();
    diagnostic.RestoreSignal();
  } catch (...) {
    // Diagnostics are best effort; reporting failure must not change exit
    // status.
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    Report("Usage: ", argv[0], " <filename>\n");
    return 1;
  }

  try {
    unplugged::Editor editor(Document{file_loader::Load(argv[1])});
    RenderCache render_cache;
    const auto frame = editor.CreateFrame();
    terminal::Output output(STDOUT_FILENO);
    terminal::RawMode raw_mode(STDIN_FILENO);
    {
      terminal::InputProtocol input_protocol(output);
      output.Clear();
      Draw(output, frame, render_cache);

      while (true) {
        const auto input = ReadKey();
        const auto* key = std::get_if<Key>(&input);
        if (!key) {
          if (std::get<KeyReadStatus>(input) == KeyReadStatus::Eof) break;
          continue;
        }
        if (!editor.HandleKey(*key)) break;
        Draw(output, editor.CreateFrame(), render_cache);
      }

      output.Clear();
      input_protocol.Restore();
    }
    raw_mode.Restore();
    output.RestoreSignal();
    return 0;
  } catch (const std::exception& error) {
    Report("Editor error: ", error.what(), "\n");
    return 1;
  }
}
