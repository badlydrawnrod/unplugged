#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <gsl/gsl>
#include <stdexcept>
#include <string>

#include "document/types/types.h"
#include "file_loader/load.h"

// Feature: file_loader/features/file_opening.feature
// Scenario: An empty file loads successfully
TEST(FileLoaderTest, LoadsAnEmptyFileSuccessfully) {
  std::string filename = testing::TempDir() + "unplugged-empty-file-XXXXXX";
  const int fd = mkstemp(filename.data());
  ASSERT_GE(fd, 0);
  const auto cleanup = gsl::finally([&] {
    close(fd);
    unlink(filename.c_str());
  });

  EXPECT_NO_THROW(EXPECT_TRUE(file_loader::Load(filename.c_str()).empty()));
}

// Feature: file_loader/features/file_opening.feature
// Scenario: A missing file reports a load failure
TEST(FileLoaderTest, RejectsMissingFile) {
  std::string filename = testing::TempDir() + "unplugged-missing-file-XXXXXX";
  const int fd = mkstemp(filename.data());
  ASSERT_GE(fd, 0);
  close(fd);
  ASSERT_EQ(unlink(filename.c_str()), 0);

  EXPECT_THROW(file_loader::Load(filename.c_str()), std::runtime_error);
}

// Feature: file_loader/features/file_opening.feature
// Scenario: A path that cannot be read as a file reports a load failure
TEST(FileLoaderTest, RejectsDirectoryAsFile) {
  std::string directory = testing::TempDir() + "unplugged-directory-XXXXXX";
  ASSERT_NE(mkdtemp(directory.data()), nullptr);
  const auto cleanup = gsl::finally([&] { rmdir(directory.c_str()); });

  EXPECT_THROW(file_loader::Load(directory.c_str()), std::runtime_error);
}

// Feature: file_loader/features/document_size.feature
// Scenario: Oversized files cannot be loaded for editing
TEST(FileLoaderTest, RejectsOversizedFileBeforeAllocatingContent) {
  // A sparse file exercises the real boundary without allocating gigabytes.
  std::string filename = testing::TempDir() + "unplugged-size-limit-XXXXXX";
  const int fd = mkstemp(filename.data());
  ASSERT_GE(fd, 0);
  const auto cleanup = gsl::finally([&] {
    close(fd);
    unlink(filename.c_str());
  });
  ASSERT_EQ(ftruncate(fd, static_cast<off_t>(kMaxDocumentBytes) + 1), 0);

  EXPECT_THROW(file_loader::Load(filename.c_str()), std::length_error);
}
