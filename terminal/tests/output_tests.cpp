#include <gtest/gtest.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>

#include "terminal/output.h"
#include "test_support/posix_endpoints.h"

namespace {
int signal_failures = 0;
class ScopedSignalFailure {
 public:
  ScopedSignalFailure() { signal_failures = 1; }
  ~ScopedSignalFailure() { signal_failures = 0; }
};
}  // namespace

extern "C" int __real_sigaction(int, const struct sigaction*,
                                struct sigaction*);
extern "C" int __wrap_sigaction(int signal, const struct sigaction* action,
                                struct sigaction* saved) {
  if (signal == SIGPIPE && action && signal_failures > 0) {
    --signal_failures;
    errno = EIO;
    return -1;
  }
  return __real_sigaction(signal, action, saved);
}

namespace {
using unplugged::test_support::Pipe;

static_assert(!std::is_copy_constructible_v<terminal::Output>);
static_assert(!std::is_move_constructible_v<terminal::Output>);
static_assert(std::is_nothrow_destructible_v<terminal::Output>);

std::string ReadAvailable(int fd) {
  std::array<char, 256> buffer{};
  const int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    throw std::system_error(errno, std::generic_category());
  const ssize_t count = read(fd, buffer.data(), buffer.size());
  if (count < 0) throw std::system_error(errno, std::generic_category());
  return std::string(buffer.data(), static_cast<size_t>(count));
}

// Feature: terminal/features/terminal_output.feature
// Scenario: A flushed frame emits cursor movement and row text in order
TEST(TerminalTest, FlushEmitsRenderingCommandsInOrder) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  output.MoveTo(2, 5);
  output.PutString("hello");
  output.PutChar('!');
  output.ClearToEol();
  output.MoveToStartOfNextLine();
  output.Flush();
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[2;5Hhello!\x1b[0K\x1b[E");
}

TEST(TerminalTest, ClearFlushesScreenAndHomeCommands) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  output.Clear();
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[2J\x1b[1;1H");
}

TEST(TerminalTest, FlushPreservesEmbeddedNulAndDoesNotReplayRows) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  output.PutString(std::string_view("a\0b", 3));
  output.Flush();
  EXPECT_EQ(ReadAvailable(pipe.Reader()), std::string("a\0b", 3));
  output.Flush();
  output.PutString("x");
  output.Flush();
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "x");
}

// Feature: terminal/features/terminal_output.feature
// Scenario: A broken output pipe reports an error instead of terminating
TEST(TerminalTest, BrokenPipeReportsEpipeWithSignalRestoredAfterScope) {
  struct sigaction original{};
  ASSERT_EQ(sigaction(SIGPIPE, nullptr, &original), 0);
  Pipe pipe;
  pipe.CloseReader();
  {
    terminal::Output output(pipe.Writer());
    output.PutString("frame");
    try {
      output.Flush();
      FAIL() << "Expected broken pipe error";
    } catch (const std::system_error& error) {
      EXPECT_EQ(error.code(), std::error_code(EPIPE, std::generic_category()));
    }
    // Failure discarded the submitted frame; an empty flush is now a no-op.
    EXPECT_NO_THROW(output.Flush());
  }
  struct sigaction restored{};
  ASSERT_EQ(sigaction(SIGPIPE, nullptr, &restored), 0);
  EXPECT_EQ(restored.sa_handler, original.sa_handler);
  for (int signal = 1; signal < NSIG; ++signal) {
    EXPECT_EQ(sigismember(&restored.sa_mask, signal),
              sigismember(&original.sa_mask, signal));
  }
}

TEST(TerminalTest, InvalidOutputDescriptorReportsWriteFailure) {
  terminal::Output output(-1);
  output.PutString("frame");
  EXPECT_THROW(output.Flush(), std::system_error);
}

TEST(TerminalTest, SignalRestoreIsExplicitAndIdempotent) {
  Pipe pipe;
  struct sigaction original{};
  ASSERT_EQ(sigaction(SIGPIPE, nullptr, &original), 0);
  terminal::Output output(pipe.Writer());
  output.PutString("pending");
  output.RestoreSignal();
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "pending");
  output.RestoreSignal();
  EXPECT_THROW(output.PutString("closed"), std::logic_error);
  struct sigaction restored{};
  ASSERT_EQ(sigaction(SIGPIPE, nullptr, &restored), 0);
  EXPECT_EQ(restored.sa_handler, original.sa_handler);
}
TEST(TerminalTest, SignalAcquisitionFailureDoesNotPublishAnOutputOwner) {
  Pipe pipe;
  ScopedSignalFailure failure;
  EXPECT_THROW(terminal::Output{pipe.Writer()}, std::system_error);
}

TEST(TerminalTest, FailedSignalRestoreRetainsDestructorCleanup) {
  Pipe pipe;
  struct sigaction original{};
  ASSERT_EQ(sigaction(SIGPIPE, nullptr, &original), 0);
  {
    terminal::Output output(pipe.Writer());
    ScopedSignalFailure failure;
    EXPECT_THROW(output.RestoreSignal(), std::system_error);
  }
  struct sigaction restored{};
  ASSERT_EQ(sigaction(SIGPIPE, nullptr, &restored), 0);
  EXPECT_EQ(restored.sa_handler, original.sa_handler);
}
}  // namespace
