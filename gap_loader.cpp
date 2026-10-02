#include "gap_loader.h"

#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#include "gap_buffer.h"

namespace gap_loader {

std::vector<uint8_t> Load(const char *filename) {
  // Read the entire file into a buffer.
  if (std::ifstream is{filename, std::ios::binary | std::ios::ate}) {
    auto size = is.tellg();
    if (size < 0) {
      return {};
    }
    std::vector<std::uint8_t> buffer(static_cast<size_t>(size));
    is.seekg(0);
    if (is.read(reinterpret_cast<char *>(buffer.data()), size)) {
      return buffer;
    }
  }

  return {};
}

// Create a GapBuffer populated from a file.
GapBuffer From(const char *filename) {
  std::vector<std::uint8_t> data = Load(filename);
  return GapBuffer(std::move(data));
}

}  // namespace gap_loader
