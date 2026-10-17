# Checked descriptor I/O

`//terminal_io:api` provides POSIX descriptor operations through
[`terminal_io/io.h`](include/terminal_io/io.h). The functions borrow their
descriptors and never close them. Syscall failures throw `std::system_error`,
and interrupted read, write, poll, or terminal checks are retried.

- `ReadSome()` reads into a caller-owned buffer and returns the number of bytes
  read. A zero result keeps its endpoint-specific meaning, such as timeout or
  EOF.
- `HasHangup()` checks for a hangup without waiting.
- `IsTerminal()` distinguishes a nonterminal descriptor from syscall failure.
- `WriteAll()` handles partial writes until all bytes are delivered; a zero
  write is an I/O error. Callers that need broken pipes reported as `EPIPE`
  must suppress `SIGPIPE`.

The [syscall tests](tests/) verify observable bytes and error results while
injecting syscall outcomes.
