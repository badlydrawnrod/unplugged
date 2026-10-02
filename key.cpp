#include "key.h"

// Encodes the codepoint to UTF-8. Only valid when IsText() == true.
std::string Key::Text() const {
  if (!isText) return {};
  std::string out;
  uint32_t cp = codepoint;
  if (cp <= 0x7f) {
    out += static_cast<char>(cp);
  } else if (cp <= 0x7ff) {
    out += static_cast<char>(0xc0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp <= 0xffff) {
    out += static_cast<char>(0xe0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  }
  return out;
}
