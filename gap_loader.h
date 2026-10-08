#pragma once

#include <cstdint>
#include <vector>

#include "document/internal/gap_buffer/gap_buffer.h"

namespace gap_loader {
// Load a file into a byte buffer.
// Throws std::length_error if its size exceeds kMaxDocumentBytes.
std::vector<uint8_t> Load(const char* filename);

// Create a gap buffer by populating it from a file.
GapBuffer From(const char* filename);
}  // namespace gap_loader
