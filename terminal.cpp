#include "terminal.h"

#include <format>
#include <iostream>

namespace terminal {

void Clear() {
  // Clear the screen.
  std::cout << "\x1b[2J";

  // Move to top left.
  std::cout << "\x1b[1;1H";
  std::flush(std::cout);
}

void ClearToEol() {
  // Clear to the end of the line.
  std::cout << "\x1b[0K";
}

void MoveToStartOfNextLine() {
  // Cursor to beginning of next line.
  std::cout << "\x1b[E";
}

void MoveTo(size_t row, size_t column) {
  // Cursor to row, column (both are 1-based).
  std::cout << std::format("\x1b[{};{}H", row, column);
}

void PutChar(char c) { std::cout << c; }

void PutString(std::string_view sv) { std::cout << sv; }

void Flush() { std::flush(std::cout); }
}  // namespace terminal
