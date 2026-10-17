# Terminal key decoding

`//key_decoder:api` provides `unplugged::DecodeKey()` in
[`key_decoder/decoder.h`](include/key_decoder/decoder.h). It reads from a
[`ByteSource`](ports/byte_source/README.md) and returns one decoded `Key`, or
`std::nullopt` when the input does not produce a key. The decoder is independent
of terminal and POSIX I/O.

Each call consumes only the bytes it examines and keeps no parser state for a
later call. An initial timeout, unsupported input, or interrupted sequence
returns no key. Escape followed by timeout produces Escape; Escape followed by
EOF or Error produces no key. UTF-8 scalar validity is not fully checked;
legacy Alt fallback accepts only ASCII and two- or three-byte UTF-8; protocol
event suffixes are ignored; and the current modifier parser cannot represent
field 256. These limitations are part of the current API description and should
change only with an explicit behavior update.

The [decoding scenarios](features/decoding.feature) and
[decoder tests](tests/) cover UTF-8 text, selected legacy and kitty control
forms, and Escape handling. Byte acquisition behavior belongs to the
[byte-source port](ports/byte_source/README.md).
