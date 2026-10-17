#pragma once

#include <cstdint>
#include <vector>

namespace file_loader {
// Load a file into a byte buffer.
// Throws std::length_error if its size exceeds kMaxDocumentBytes.
// Throws std::runtime_error if the file cannot be opened, sized, or read.
std::vector<uint8_t> Load(const char* filename);

}  // namespace file_loader
