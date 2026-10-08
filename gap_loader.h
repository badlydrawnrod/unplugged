#pragma once

#include <cstdint>
#include <vector>

namespace gap_loader {
// Load a file into a byte buffer.
// Throws std::length_error if its size exceeds kMaxDocumentBytes.
std::vector<uint8_t> Load(const char* filename);

}  // namespace gap_loader
