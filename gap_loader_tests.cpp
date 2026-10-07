#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <gsl/gsl>
#include <stdexcept>
#include <string>

#include "gap_loader.h"

// Feature: features/document_size.feature
// Scenario: Oversized files cannot be loaded for editing
TEST(GapLoaderTest, RejectsOversizedFileBeforeAllocatingContent) {
  // A sparse file exercises the real boundary without allocating gigabytes.
  std::string filename = testing::TempDir() + "unplugged-size-limit-XXXXXX";
  const int fd = mkstemp(filename.data());
  ASSERT_GE(fd, 0);
  const auto cleanup = gsl::finally([&] {
    close(fd);
    unlink(filename.c_str());
  });
  ASSERT_EQ(ftruncate(fd, static_cast<off_t>(kMaxDocumentBytes) + 1), 0);

  EXPECT_THROW(gap_loader::Load(filename.c_str()), std::length_error);
}
