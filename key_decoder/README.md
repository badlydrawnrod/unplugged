# Terminal key decoding

## Decode one key

Depend on `//key_decoder:api` and call `unplugged::DecodeKey()` from
[`key_decoder/decoder.h`](include/key_decoder/decoder.h). It reads from a
[`ByteSource`](ports/byte_source/README.md) and returns one decoded `Key`, or
`std::nullopt` when the input does not produce a key. The decoder has no
terminal or POSIX I/O dependency.

## Input and parser behavior

Each call consumes only the bytes it examines and keeps no parser state for a
later call. An initial timeout, unsupported input, or interrupted sequence
returns no key. Escape followed by timeout produces Escape; Escape followed by
EOF or Error produces no key.

## Known decoding limits

UTF-8 scalar validity is not fully checked. Legacy Alt fallback accepts ASCII
and two- or three-byte UTF-8. Protocol event suffixes are ignored, and the
current modifier parser cannot represent field 256. These are current behavior
limits; change them only through an explicit behavior update.

The [decoding scenarios](features/decoding.feature) and
[decoder tests](tests/) cover UTF-8 text, selected legacy and kitty control
forms, and Escape handling. For byte status and adapter behavior, see the
[byte-source port contract](ports/byte_source/README.md).
