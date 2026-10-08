# Decoder-owned byte acquisition contract

Decoder and adapters depend directly on `//key_decoder/ports/byte_source:api`.
The supported [header](../../byte_source.h) remains at the port root;
[visibility](../../BUILD.bazel) restricts consumers to decoder and terminal
composition/tests. The decoder has no terminal or POSIX I/O dependency.

Each successful acquisition consumes one byte in source order, preserving all
eight bits, including NUL. Timeout, EOF, and Error are distinct from every byte;
status outcomes consume no byte. Timeout means this acquisition found no byte,
and later calls may supply bytes. EOF means the input has ended. Error represents
failure for implementations that report it as a value. Timing and exception
policies belong to adapters; decoding does not choose acquisition timeouts.

The terminal-owned [FD adapter](../../../../../terminal/internal/fd_byte_source/fd_byte_source.h)
borrows a descriptor and reads one byte at a time. A zero terminal read without
hangup is Timeout; zero reads with hangup or from nonterminals are EOF. Syscall
failures throw `std::system_error`; this adapter never returns Error. Adapter
exceptions propagate through decoding. Deterministic decoder test streams can
supply statuses directly without real I/O or clock dependencies.

[DecodeKey()](../../../../include/key_decoder/decoder.h) discards consumed
prefixes on interruption and retains no parser state between calls. Escape
followed by Timeout is bare Escape; Escape followed by EOF or Error yields no
key. Its API documents existing UTF-8, legacy Alt, modifier, and event-suffix
limitations; changing them requires a separate behavioral increment.

## Verification

The test-only `:conformance` target provides shared checks for byte preservation,
finite EOF, and timeout recovery. [Port features](../../features/acquisition.feature)
bind to both deterministic decoder and FD adapter tests. Hangup, syscall
exceptions, and synthetic Error expectations remain adapter-specific.
Run the decoder and FD adapter suites together:

```sh
bazel test //key_decoder/tests:decoder_tests //terminal/tests:fd_byte_source_tests
bazel test -c dbg //key_decoder/tests:decoder_tests //terminal/tests:fd_byte_source_tests
```

Run `python3 tools/verify.py` from the repository root for acceptance
traceability. Tests consume supported APIs and the narrow conformance helper;
they do not rely on acquisition call counts or parser representation.
