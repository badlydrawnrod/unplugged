#include "input_protocol.h"

#include <unistd.h>

#include <format>
#include <span>

#include "terminal_io/io.h"

void EnableInputProtocol(terminal::Output& output) {
  // Probe for kitty input protocol by querying the current values of the
  // flags.
  output.PutString("\x1b[?u");
  output.Flush();

  // Check that we got back CSI ? flags u
  // TODO: Remove this hack!
  char data[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
  const auto n = terminal::io::ReadSome(STDIN_FILENO, std::span(data));
  output.PutString(
      std::format("n = {}, {:02x}{:02x}{:02x}{:02x}{:02x}{:02x}\r\n", n,
                  data[0], data[1], data[2], data[3], data[4], data[5]));
  if (n >= 5 && data[0] == '\x1b' && data[1] == '[' && data[2] == '?' &&
      data[n - 1] == 'u') {
    output.PutString("\r\nProbably kitty\r\n");
    // Progressive enhancement: disambiguate escape codes. That's all we're
    // going to support.
    output.PutString("\x1b[>1u");
  } else {
    output.PutString("\r\nNo kitty\r\n");
  }
  output.Flush();
}

void DisableInputProtocol(terminal::Output& output) {
  output.PutString("\x1b[<u");
  output.Flush();
}
