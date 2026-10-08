#include "terminal.h"

#include <cerrno>
#include <format>
#include <stdexcept>
#include <system_error>

#include "terminal_io/io.h"

namespace terminal {

Output::Output(int fd) : fd_(fd) {
  struct sigaction ignored{};
  ignored.sa_handler = SIG_IGN;
  if (sigemptyset(&ignored.sa_mask) < 0 ||
      sigaction(SIGPIPE, &ignored, &saved_sigpipe_) < 0) {
    throw std::system_error(errno, std::generic_category(), "suppress SIGPIPE");
  }
  signal_active_ = true;
}

Output::~Output() noexcept {
  if (signal_active_) sigaction(SIGPIPE, &saved_sigpipe_, nullptr);
}

void Output::RestoreSignal() {
  if (!signal_active_) return;
  Flush();
  if (sigaction(SIGPIPE, &saved_sigpipe_, nullptr) < 0) {
    throw std::system_error(errno, std::generic_category(), "restore SIGPIPE");
  }
  signal_active_ = false;
}

void Output::Clear() {
  PutString("\x1b[2J\x1b[1;1H");
  Flush();
}
void Output::ClearToEol() { PutString("\x1b[0K"); }
void Output::MoveToStartOfNextLine() { PutString("\x1b[E"); }
void Output::MoveTo(size_t row, size_t column) {
  PutString(std::format("\x1b[{};{}H", row, column));
}
void Output::CheckActive() const {
  if (!signal_active_)
    throw std::logic_error("terminal output session has ended");
}
void Output::PutChar(char c) {
  CheckActive();
  pending_.push_back(c);
}
void Output::PutString(std::string_view text) {
  CheckActive();
  pending_.append(text);
}
void Output::Flush() {
  if (pending_.empty()) return;
  CheckActive();
  try {
    io::WriteAll(fd_, pending_);
  } catch (...) {
    pending_.clear();
    throw;
  }
  pending_.clear();
}

}  // namespace terminal
