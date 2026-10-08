#include <gtest/gtest.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <system_error>

#include "key_decoder/ports/byte_source/conformance.h"
#include "terminal/internal/fd_byte_source/fd_byte_source.h"
#include "test_support/posix_endpoints.h"

namespace {
namespace conformance = unplugged::byte_source_conformance;
using unplugged::test_support::Pipe;
using unplugged::test_support::PseudoTerminal;

// Feature: key_decoder/ports/byte_source/features/acquisition.feature
// Scenario: Finite input preserves every byte before EOF
TEST(FdByteSourceTest, PreservesAllBytesThenReportsEof) {
  Pipe pipe;
  const auto bytes = conformance::AllBytes();
  ASSERT_EQ(write(pipe.Writer(), bytes.data(), bytes.size()),
            static_cast<ssize_t>(bytes.size()));
  pipe.CloseWriter();
  terminal::FdByteSource source(pipe.Reader());
  conformance::ExpectOrderedBytes(source, bytes);
  EXPECT_FALSE(source.ReachedEnd());
  conformance::ExpectEof(source);
  EXPECT_TRUE(source.ReachedEnd());
}

// Feature: key_decoder/ports/byte_source/features/acquisition.feature
// Scenario: Input can continue after a timeout
TEST(FdByteSourceTest, TerminalTimeoutAllowsLaterBytesWithoutSleeping) {
  PseudoTerminal terminal;
  termios attributes{};
  ASSERT_EQ(tcgetattr(terminal.Slave(), &attributes), 0);
  cfmakeraw(&attributes);
  attributes.c_cc[VMIN] = 0;
  attributes.c_cc[VTIME] = 0;
  ASSERT_EQ(tcsetattr(terminal.Slave(), TCSANOW, &attributes), 0);
  terminal::FdByteSource source(terminal.Slave());
  conformance::ExpectTimeout(source);
  EXPECT_FALSE(source.ReachedEnd());
  const std::array<uint8_t, 3> bytes{0, 255, 'x'};
  ASSERT_EQ(write(terminal.Master(), bytes.data(), bytes.size()),
            static_cast<ssize_t>(bytes.size()));
  // Wait for input deterministically using blocking byte acquisition, rather
  // than assuming the PTY line discipline has delivered the write immediately.
  attributes.c_cc[VMIN] = 1;
  ASSERT_EQ(tcsetattr(terminal.Slave(), TCSANOW, &attributes), 0);
  conformance::ExpectOrderedBytes(source, bytes);
}

// Feature: terminal/features/terminal_input.feature
// Scenario: A terminal hangup ends input
TEST(FdByteSourceTest, TerminalHangupReportsEof) {
  PseudoTerminal terminal;
  terminal.Disconnect();
  terminal::FdByteSource source(terminal.Slave());
  conformance::ExpectEof(source);
  EXPECT_TRUE(source.ReachedEnd());
}

// Feature: terminal/features/terminal_input.feature
// Scenario: Acquisition failures report an error
TEST(FdByteSourceTest, InvalidDescriptorThrowsInsteadOfReturningError) {
  terminal::FdByteSource source(-1);
  try {
    source.ReadByte();
    FAIL() << "Expected read failure";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(EBADF, std::generic_category()));
  }
  EXPECT_FALSE(source.ReachedEnd());
}

}  // namespace
