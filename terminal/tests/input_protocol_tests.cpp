#include <gtest/gtest.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>

#include "terminal/input_protocol.h"
#include "terminal/output.h"
#include "test_support/posix_endpoints.h"

namespace {
using unplugged::test_support::Pipe;

static_assert(!std::is_copy_constructible_v<terminal::InputProtocol>);
static_assert(!std::is_copy_assignable_v<terminal::InputProtocol>);
static_assert(!std::is_move_constructible_v<terminal::InputProtocol>);
static_assert(!std::is_move_assignable_v<terminal::InputProtocol>);
static_assert(std::is_nothrow_destructible_v<terminal::InputProtocol>);

std::string ReadAvailable(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    throw std::system_error(errno, std::generic_category());
  std::array<char, 256> buffer{};
  const ssize_t count = read(fd, buffer.data(), buffer.size());
  if (count < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return {};
    throw std::system_error(errno, std::generic_category());
  }
  return std::string(buffer.data(), static_cast<size_t>(count));
}

// Feature: terminal/features/input_protocol.feature
// Scenario: Startup requests disambiguation without a support probe
TEST(InputProtocolTest, StartupFlushesOnlyTheEnhancementRequest) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  terminal::InputProtocol protocol(output);
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[>1u");
}

// Feature: terminal/features/input_protocol.feature
// Scenario: Normal shutdown restores the previous keyboard mode once
TEST(InputProtocolTest, ExplicitRestoreDisarmsCleanup) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  {
    terminal::InputProtocol protocol(output);
    protocol.Restore();
    protocol.Restore();
    EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[>1u\x1b[<u");
  }
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "");
}

TEST(InputProtocolTest, NormalScopeExitRestoresPreviousMode) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  {
    terminal::InputProtocol protocol(output);
  }
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[>1u\x1b[<u");
}

// Feature: terminal/features/input_protocol.feature
// Scenario: An application failure restores the previous keyboard mode
TEST(InputProtocolTest, ExceptionUnwindingRestoresPreviousMode) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  EXPECT_THROW(
      {
        terminal::InputProtocol protocol(output);
        throw std::runtime_error("application failure");
      },
      std::runtime_error);
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[>1u\x1b[<u");
}

TEST(InputProtocolTest, NestedSessionsRestoreInStackOrder) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  {
    terminal::InputProtocol outer(output);
    {
      terminal::InputProtocol inner(output);
    }
    EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[>1u\x1b[>1u\x1b[<u");
  }
  EXPECT_EQ(ReadAvailable(pipe.Reader()), "\x1b[<u");
}

TEST(InputProtocolTest, FailedAcquisitionPropagatesOriginalError) {
  terminal::Output output(-1);
  try {
    terminal::InputProtocol protocol(output);
    FAIL() << "Expected acquisition failure";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(EBADF, std::generic_category()));
  }
}

// Feature: terminal/features/input_protocol.feature
// Scenario: A reset failure is reported without preventing cleanup
TEST(InputProtocolTest, FailedRestoreReportsErrorAndDestructorDoesNotThrow) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  {
    terminal::InputProtocol protocol(output);
    pipe.CloseReader();
    try {
      protocol.Restore();
      FAIL() << "Expected reset failure";
    } catch (const std::system_error& error) {
      EXPECT_EQ(error.code(), std::error_code(EPIPE, std::generic_category()));
    }
  }
}

TEST(InputProtocolTest, CleanupFailurePreservesPrimaryException) {
  Pipe pipe;
  terminal::Output output(pipe.Writer());
  try {
    terminal::InputProtocol protocol(output);
    pipe.CloseReader();
    throw std::runtime_error("primary failure");
  } catch (const std::runtime_error& error) {
    EXPECT_STREQ(error.what(), "primary failure");
  }
}
}  // namespace
