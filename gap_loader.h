#pragma once

#include <cstdint>
#include <vector>

#include "gap_buffer.h"

namespace gap_loader {
// Load a file into a byte buffer.
std::vector<uint8_t> Load(const char* filename);

// Create a gap buffer by populating it from a file.
GapBuffer From(const char* filename);
}  // namespace gap_loader
