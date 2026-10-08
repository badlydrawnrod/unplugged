#pragma once

#include <signal.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace terminal {

// Buffered terminal output to a borrowed descriptor. Flush reports write errors
// as std::system_error and handles partial/interrupted writes. Failed flushes
// discard pending output so cleanup cannot replay a partially written frame.
// Owns scoped process-wide SIGPIPE suppression; scopes must nest in LIFO order.
class Output {
 public:
  explicit Output(int fd);
  ~Output() noexcept;
  Output(const Output&) = delete;
  Output& operator=(const Output&) = delete;
  Output(Output&&) = delete;
  Output& operator=(Output&&) = delete;

  void Clear();
  void ClearToEol();
  void MoveToStartOfNextLine();
  void MoveTo(size_t row, size_t column);
  void PutChar(char c);
  void PutString(std::string_view text);
  void Flush();
  // Checked teardown flushes pending bytes and ends the output session. Further
  // output requests throw std::logic_error. The destructor restores signal
  // handling best effort without flushing.
  void RestoreSignal();

 private:
  void CheckActive() const;
  int fd_;
  std::string pending_;
  struct sigaction saved_sigpipe_{};
  bool signal_active_ = false;
};

}  // namespace terminal
