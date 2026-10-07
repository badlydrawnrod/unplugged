#pragma once

#include <cstddef>

namespace unplugged::testing {

// Fail allocation on this thread after the specified number of successes.
// Keep the scope around the tested call, outside fixtures and assertions.
class FailAllocationAfter {
 public:
  explicit FailAllocationAfter(size_t successful_allocations) noexcept;
  ~FailAllocationAfter();

  FailAllocationAfter(const FailAllocationAfter&) = delete;
  FailAllocationAfter& operator=(const FailAllocationAfter&) = delete;
};

}  // namespace unplugged::testing
