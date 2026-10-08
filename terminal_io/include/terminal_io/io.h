#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace terminal::io {

// Borrowed descriptors; syscall failures throw std::system_error. EINTR
// retries. A zero read retains its endpoint-specific meaning (timeout or EOF).
size_t ReadSome(int fd, std::span<char> bytes);
bool HasHangup(int fd);
bool IsTerminal(int fd);

// Writes the complete string, handling partial progress. A zero write is an
// I/O error. Callers must suppress SIGPIPE if they need EPIPE as an exception.
void WriteAll(int fd, std::string_view bytes);

}  // namespace terminal::io
