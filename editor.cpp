#include "editor_core/editor.h"

#include <unistd.h>

#include <exception>
#include <gsl/gsl>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "document.h"
#include "gap_loader.h"
#include "input_protocol.h"
#include "raw_mode.h"
#include "read_key.h"
#include "terminal.h"

namespace {

struct RenderCache {
  std::vector<std::string> last_drawn_rows{};
  bool has_last_drawn_rows = false;
};

void Draw(const unplugged::EditorFrame& frame, RenderCache& cache) {
  std::vector<std::string> next_rows;
  next_rows.reserve(frame.rows.size());

  for (size_t row = 0; row < frame.rows.size(); ++row) {
    next_rows.push_back(frame.rows[row]);

    if (!cache.has_last_drawn_rows || row >= cache.last_drawn_rows.size() ||
        next_rows[row] != cache.last_drawn_rows[row]) {
      terminal::MoveTo(row + 1, 1);
      terminal::PutString(next_rows[row]);
      terminal::ClearToEol();
    }
  }

  cache.last_drawn_rows = std::move(next_rows);
  cache.has_last_drawn_rows = true;

  terminal::MoveTo(frame.cursor_row, frame.cursor_column);
  terminal::Flush();
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>\n";
    return 1;
  }

  try {
    unplugged::Editor editor(Document{gap_loader::Load(argv[1])});
    RenderCache render_cache;
    const auto frame = editor.CreateFrame();
    terminal::RawMode raw_mode(STDIN_FILENO);
    {
      const auto protocol_cleanup = gsl::finally(DisableInputProtocol);
      EnableInputProtocol();
      terminal::Clear();
      Draw(frame, render_cache);

      while (true) {
        const auto key = ReadKey();
        if (!key) {
          if (!isatty(STDIN_FILENO)) {
            break;
          }
          continue;
        }
        if (!editor.HandleKey(*key)) {
          break;
        }
        Draw(editor.CreateFrame(), render_cache);
      }

      terminal::Clear();
    }
    raw_mode.Restore();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Editor error: " << error.what() << '\n';
    return 1;
  }
}
