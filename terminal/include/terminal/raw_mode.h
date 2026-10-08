#pragma once

#include <termios.h>

namespace terminal {

// Borrows an open terminal descriptor, which must outlive the scope. Each scope
// saves its own attributes; nested scopes on the same terminal restore in LIFO
// order. Does not read input, write output, or negotiate a keyboard protocol.
class RawMode {
 public:
  // Throws std::system_error when reading or applying terminal attributes
  // fails.
  explicit RawMode(int fd);
  ~RawMode() noexcept;

  RawMode(const RawMode&) = delete;
  RawMode& operator=(const RawMode&) = delete;
  RawMode(RawMode&&) = delete;
  RawMode& operator=(RawMode&&) = delete;

  // Restores once; subsequent calls are no-ops. Failure throws
  // std::system_error and leaves restoration pending, so the destructor can
  // retry. Call explicitly to report shutdown errors; the destructor performs
  // best-effort restoration.
  void Restore();

 private:
  int fd_;
  termios saved_{};
  bool active_ = false;
};

}  // namespace terminal
