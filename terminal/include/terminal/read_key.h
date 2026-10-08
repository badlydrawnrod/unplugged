#pragma once

#include <variant>

#include "key/key.h"

enum class KeyReadStatus { NoKey, Eof };
using KeyReadResult = std::variant<Key, KeyReadStatus>;

// Acquires input and delegates decoding. NoKey covers timeouts and discarded
// unsupported/incomplete sequences; EOF remains distinct, including a terminal
// hangup. Acquisition errors throw std::system_error rather than becoming
// NoKey. Borrowed descriptor; the no-argument overload reads stdin.
KeyReadResult ReadKey(int fd);
KeyReadResult ReadKey();
