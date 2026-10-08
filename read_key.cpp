#include "read_key.h"

#include <unistd.h>

#include "key_decoder/decoder.h"

namespace {

class StdinByteSource final : public unplugged::ByteSource {
 public:
  unplugged::ByteReadResult ReadByte() override {
    uint8_t byte = 0;
    const auto count = read(STDIN_FILENO, &byte, 1);
    if (count == 1) {
      return byte;
    }
    if (count == 0) {
      return isatty(STDIN_FILENO) ? unplugged::ByteReadStatus::Timeout
                                  : unplugged::ByteReadStatus::Eof;
    }
    return unplugged::ByteReadStatus::Error;
  }
};

}  // namespace

std::optional<Key> ReadKey() {
  StdinByteSource source;
  return unplugged::DecodeKey(source);
}
