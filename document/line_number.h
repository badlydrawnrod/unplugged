#pragma once

#include <cstdint>

namespace unplugged::document {
// Logical-line ordinals belong to the document contract, independently of
// storage.
using LineNumber = std::uint32_t;
}  // namespace unplugged::document
