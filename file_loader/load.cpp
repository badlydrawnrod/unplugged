#include "file_loader/load.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "document/types/types.h"

namespace file_loader {

std::vector<uint8_t> Load(const char *filename) {
  std::error_code status_error;
  if (!std::filesystem::is_regular_file(filename, status_error)) {
    throw std::runtime_error(std::string("cannot open file: ") + filename);
  }

  std::ifstream is{filename, std::ios::binary | std::ios::ate};
  if (!is) {
    throw std::runtime_error(std::string("cannot open file: ") + filename);
  }

  const auto size = is.tellg();
  if (!is || size < 0) {
    throw std::runtime_error(std::string("cannot determine file size: ") +
                             filename);
  }
  // Reject oversized files before narrowing the size or allocating storage.
  if (size > static_cast<std::streamoff>(kMaxDocumentBytes)) {
    throw std::length_error("file exceeds supported document-size limit");
  }

  std::vector<std::uint8_t> buffer(static_cast<size_t>(size));
  if (buffer.empty()) {
    return buffer;
  }

  is.seekg(0);
  if (!is || !is.read(reinterpret_cast<char *>(buffer.data()), size)) {
    throw std::runtime_error(std::string("cannot read file: ") + filename);
  }
  return buffer;
}

}  // namespace file_loader
