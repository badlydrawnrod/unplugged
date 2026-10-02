#pragma once

#include <string>

namespace terminal {
void Clear();
void ClearToEol();
void MoveToStartOfNextLine();
void MoveTo(size_t row, size_t column);
void PutChar(char c);
void PutString(std::string_view sv);
void Flush();
}  // namespace terminal
