#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <new>

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

template <typename Make, typename Edit, typename VerifyBefore,
          typename VerifyAfter>
void ExerciseAllocationFailures(Make make, Edit edit,
                                VerifyBefore verify_before,
                                VerifyAfter verify_after) {
  size_t failures = 0;
  // Discover allocation points rather than asserting their number or order.
  for (size_t fail_after = 0;; ++fail_after) {
    auto value = make();
    bool failed = false;
    try {
      FailAllocationAfter inject_failure(fail_after);
      edit(value);
    } catch (const std::bad_alloc&) {
      failed = true;
    }
    if (!failed) {
      verify_after(value);
      EXPECT_GT(failures, 0u);
      return;
    }
    ++failures;
    verify_before(value);
    // The same object must still support editing once allocation is available.
    edit(value);
    verify_after(value);
  }
}

}  // namespace unplugged::testing
