#include "file_loader/load.h"

#include <fstream>
#include <stdexcept>
#include <vector>

#include "document/types/types.h"

namespace file_loader {

std::vector<uint8_t> Load(const char *filename) {
  // Read the entire file into a buffer.
  if (std::ifstream is{filename, std::ios::binary | std::ios::ate}) {
    auto size = is.tellg();
    if (size < 0) {
      return {};
    }
    // Reject oversized files before narrowing the size or allocating storage.
    if (size > static_cast<std::streamoff>(kMaxDocumentBytes)) {
      throw std::length_error("file exceeds supported document-size limit");
    }
    std::vector<std::uint8_t> buffer(static_cast<size_t>(size));
    is.seekg(0);
    if (is.read(reinterpret_cast<char *>(buffer.data()), size)) {
      return buffer;
    }
  }

  return {};
}

}  // namespace file_loader
