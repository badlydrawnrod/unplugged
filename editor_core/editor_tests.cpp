#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "document_view.h"
#include "editor_core/editor.h"

namespace unplugged {
namespace {

Editor MakeEditor(std::string_view text, size_t width = 12, size_t height = 4) {
  return Editor(Document{std::vector<Byte>(text.begin(), text.end())}, width,
                height);
}

std::string Contents(const Editor& editor) {
  const auto view = editor.GetDocument().View();
  return std::string(view.begin(), view.end());
}

void Press(Editor& editor, NavigationKey key, KeyMods mods = KeyMods::None) {
  ASSERT_TRUE(editor.HandleKey(Key::Special(key, mods)));
}

void Press(Editor& editor, EditingKey key) {
  ASSERT_TRUE(editor.HandleKey(Key::Special(key)));
}

// Feature: editor_core/features/editing.feature
// Scenario: Insert text and split a line at the cursor
TEST(EditorTest, InsertsTextAndNewlineAtCursor) {
  auto editor = MakeEditor("ab");
  Press(editor, NavigationKey::Right);
  ASSERT_TRUE(editor.HandleKey(Key::MakeText('x')));
  Press(editor, EditingKey::Enter);
  EXPECT_EQ(Contents(editor), "ax\nb");
  EXPECT_EQ(editor.CursorPosition(), 3U);
  EXPECT_EQ(editor.CreateFrame().rows,
            (std::vector<std::string>{"  1 ax", "  2 b", "~", "~"}));
  EXPECT_EQ(editor.CreateFrame().cursor_row, 2U);
  EXPECT_EQ(editor.CreateFrame().cursor_column, 5U);
}

// Feature: editor_core/features/editing.feature
// Scenario: Backspace on an empty line joins only the preceding line
TEST(EditorTest, BackspaceOnEmptyLineJoinsOnlyPrecedingLine) {
  auto editor = MakeEditor("a\n\nb");
  Press(editor, NavigationKey::Right);
  Press(editor, NavigationKey::Right);
  Press(editor, EditingKey::Backspace);
  EXPECT_EQ(Contents(editor), "a\nb");
  EXPECT_EQ(editor.CursorPosition(), 1U);
  EXPECT_EQ(editor.GetDocument().NumLines(), 2U);
}

TEST(EditorTest, DeleteJoinsLinesAndDoesNotMoveCursor) {
  auto editor = MakeEditor("a\nb");
  Press(editor, NavigationKey::Right);
  Press(editor, EditingKey::Delete);
  EXPECT_EQ(Contents(editor), "ab");
  EXPECT_EQ(editor.CursorPosition(), 1U);
  Press(editor, EditingKey::Delete);
  EXPECT_EQ(Contents(editor), "a");
  EXPECT_EQ(editor.CursorPosition(), 1U);
  Press(editor, EditingKey::Delete);
  EXPECT_EQ(Contents(editor), "a");
}

TEST(EditorTest, EmptyDocumentCommandsRemainAtByteZero) {
  auto editor = MakeEditor("");
  Press(editor, EditingKey::Backspace);
  Press(editor, EditingKey::Delete);
  for (auto key : {NavigationKey::Left, NavigationKey::Right, NavigationKey::Up,
                   NavigationKey::Down, NavigationKey::Home, NavigationKey::End,
                   NavigationKey::PageUp, NavigationKey::PageDown}) {
    Press(editor, key);
    EXPECT_EQ(editor.CursorPosition(), 0U);
    EXPECT_EQ(editor.TopRow(), 0U);
  }
  const auto frame = editor.CreateFrame();
  EXPECT_EQ(frame.rows, (std::vector<std::string>{"  1 ", "~", "~", "~"}));
  EXPECT_EQ(frame.cursor_row, 1U);
  EXPECT_EQ(frame.cursor_column, 5U);
}

TEST(EditorTest, HorizontalNavigationAndDeletionRemainByteBased) {
  auto editor = MakeEditor("");
  ASSERT_TRUE(editor.HandleKey(Key::MakeText(0x00e9)));
  EXPECT_EQ(Contents(editor), "\xc3\xa9");
  EXPECT_EQ(editor.CursorPosition(), 2U);
  Press(editor, NavigationKey::Right);
  EXPECT_EQ(editor.CursorPosition(), 2U);
  Press(editor, NavigationKey::Left);
  EXPECT_EQ(editor.CursorPosition(), 1U);
  Press(editor, EditingKey::Backspace);
  EXPECT_EQ(Contents(editor), "\xa9");
  EXPECT_EQ(editor.CursorPosition(), 0U);
  Press(editor, NavigationKey::Left);
  EXPECT_EQ(editor.CursorPosition(), 0U);
}

// Feature: editor_core/features/navigation.feature
// Scenario: Home and End distinguish logical lines from the whole document
TEST(EditorTest, HomeAndEndNavigateLogicalLinesAndDocument) {
  auto editor = MakeEditor("abcdefghij\nk\n", 8, 3);
  Press(editor, NavigationKey::End);
  EXPECT_EQ(editor.CursorPosition(), 10U);
  EXPECT_EQ(editor.CreateFrame().cursor_row, 3U);
  Press(editor, NavigationKey::Home);
  EXPECT_EQ(editor.CursorPosition(), 0U);
  Press(editor, NavigationKey::End, KeyMods::Ctrl);
  EXPECT_EQ(editor.CursorPosition(), 13U);
  EXPECT_EQ(editor.TopRow(), 2U);
  EXPECT_EQ(editor.CreateFrame().cursor_row, 3U);
  EXPECT_EQ(editor.CreateFrame().rows.back(), "  3 ");
  Press(editor, NavigationKey::Home, KeyMods::Ctrl);
  EXPECT_EQ(editor.CursorPosition(), 0U);
  EXPECT_EQ(editor.TopRow(), 0U);
}

// Feature: editor_core/features/navigation.feature
// Scenario: Vertical movement restores the preferred column after a short line
TEST(EditorTest, VerticalMovementRestoresPreferredColumn) {
  auto editor = MakeEditor("abcd\nx\n\nwxyz", 12, 6);
  Press(editor, NavigationKey::Right);
  Press(editor, NavigationKey::Right);
  Press(editor, NavigationKey::Right);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CursorPosition(), 6U);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CursorPosition(), 7U);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CursorPosition(), 11U);
  EXPECT_EQ(editor.CreateFrame().cursor_column, 8U);
  Press(editor, NavigationKey::Up);
  Press(editor, NavigationKey::Up);
  Press(editor, NavigationKey::Up);
  EXPECT_EQ(editor.CursorPosition(), 3U);
}

TEST(EditorTest, HorizontalMovementResetsPreferredColumn) {
  auto editor = MakeEditor("abcd\nx\nwxyz", 12, 5);
  Press(editor, NavigationKey::End);
  Press(editor, NavigationKey::Down);
  Press(editor, NavigationKey::Left);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CursorPosition(), 7U);
  EXPECT_EQ(editor.CreateFrame().cursor_column, 5U);
}

// Feature: editor_core/features/navigation.feature
// Scenario: Crossing a viewport boundary scrolls one row and selects its start
TEST(EditorTest, VerticalBoundaryScrollSelectsRowStart) {
  auto editor = MakeEditor("aa\nbb\ncc\ndd", 12, 2);
  Press(editor, NavigationKey::Right);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CursorPosition(), 4U);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.TopRow(), 1U);
  EXPECT_EQ(editor.CursorPosition(), 6U);
  EXPECT_EQ(editor.CreateFrame().rows,
            (std::vector<std::string>{"  2 bb", "  3 cc"}));
  Press(editor, NavigationKey::Up);
  Press(editor, NavigationKey::Up);
  EXPECT_EQ(editor.TopRow(), 0U);
  EXPECT_EQ(editor.CursorPosition(), 0U);
}

// Feature: editor_core/features/navigation.feature
// Scenario: Page navigation overlaps one row and stops at document ends
TEST(EditorTest, PageNavigationMovesByOverlappingPagesAndClampsAtEnds) {
  auto editor = MakeEditor("a\nb\nc\nd\ne", 12, 3);
  Press(editor, NavigationKey::PageDown);
  EXPECT_EQ(editor.TopRow(), 2U);
  EXPECT_EQ(editor.CursorPosition(), 4U);
  Press(editor, NavigationKey::PageDown);
  EXPECT_EQ(editor.TopRow(), 4U);
  EXPECT_EQ(editor.CursorPosition(), 8U);
  Press(editor, NavigationKey::PageDown);
  EXPECT_EQ(editor.TopRow(), 4U);
  Press(editor, NavigationKey::PageUp);
  EXPECT_EQ(editor.TopRow(), 2U);
  EXPECT_EQ(editor.CursorPosition(), 4U);
  Press(editor, NavigationKey::PageUp);
  Press(editor, NavigationKey::PageUp);
  EXPECT_EQ(editor.TopRow(), 0U);
  EXPECT_EQ(editor.CursorPosition(), 0U);
}

// Feature: editor_core/features/navigation.feature
// Scenario: Moving across a wrap boundary keeps the cursor visible
TEST(EditorTest, WrappedNavigationAndCursorVisibility) {
  auto editor = MakeEditor("abcdefghi", 8, 2);
  EXPECT_EQ(editor.CreateFrame().rows,
            (std::vector<std::string>{"  1 abcd", "    efgh"}));
  for (int i = 0; i < 4; ++i) Press(editor, NavigationKey::Right);
  EXPECT_EQ(editor.CreateFrame().cursor_row, 2U);
  EXPECT_EQ(editor.CreateFrame().cursor_column, 5U);
  for (int i = 0; i < 4; ++i) Press(editor, NavigationKey::Right);
  EXPECT_EQ(editor.TopRow(), 1U);
  EXPECT_EQ(editor.CreateFrame().rows,
            (std::vector<std::string>{"    efgh", "    i"}));
  EXPECT_EQ(editor.CreateFrame().cursor_row, 2U);
  Press(editor, NavigationKey::Home);
  EXPECT_EQ(editor.TopRow(), 0U);
}

TEST(EditorTest, ExactWidthEndUsesLastVisibleCellWithoutExtraRow) {
  auto editor = MakeEditor("abcd", 8, 3);
  Press(editor, NavigationKey::End);
  const auto frame = editor.CreateFrame();
  EXPECT_EQ(frame.rows, (std::vector<std::string>{"  1 abcd", "~", "~"}));
  EXPECT_EQ(frame.cursor_row, 1U);
  EXPECT_EQ(frame.cursor_column, 8U);
}

TEST(EditorTest, EditsKeepCursorVisibleAndNormalizeViewportAfterDeletion) {
  auto editor = MakeEditor("", 8, 1);
  for (char ch : std::string("abcdef")) {
    ASSERT_TRUE(editor.HandleKey(Key::MakeText(ch)));
  }
  EXPECT_EQ(editor.TopRow(), 1U);
  EXPECT_EQ(editor.CreateFrame().rows.front(), "    ef");
  for (int i = 0; i < 6; ++i) Press(editor, EditingKey::Backspace);
  EXPECT_EQ(Contents(editor), "");
  EXPECT_EQ(editor.TopRow(), 0U);
  EXPECT_EQ(editor.CreateFrame().cursor_row, 1U);
  EXPECT_EQ(editor.CreateFrame().rows.front(), "  1 ");
}

TEST(EditorTest, EditingResetsPreferredColumn) {
  auto editor = MakeEditor("abcd\nx\nwxyz", 12, 5);
  Press(editor, NavigationKey::End);
  Press(editor, NavigationKey::Down);
  ASSERT_TRUE(editor.HandleKey(Key::MakeText('y')));
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CreateFrame().cursor_column, 7U);
}

TEST(EditorTest, SingleRowViewportPagesByOneRow) {
  auto editor = MakeEditor("a\nb\nc", 12, 1);
  Press(editor, NavigationKey::PageDown);
  EXPECT_EQ(editor.TopRow(), 1U);
  EXPECT_EQ(editor.CursorPosition(), 2U);
  Press(editor, NavigationKey::PageUp);
  EXPECT_EQ(editor.TopRow(), 0U);
  EXPECT_EQ(editor.CursorPosition(), 0U);
}

TEST(EditorTest, PreservesRowStartSelectionAtDocumentBoundaries) {
  auto editor = MakeEditor("ab\ncd", 12, 2);
  Press(editor, NavigationKey::Right);
  Press(editor, NavigationKey::Up);
  EXPECT_EQ(editor.CursorPosition(), 0U);
  Press(editor, NavigationKey::End, KeyMods::Ctrl);
  Press(editor, NavigationKey::Down);
  EXPECT_EQ(editor.CursorPosition(), 3U);
}

TEST(EditorTest, FrameSnapshotsSurviveSubsequentEdits) {
  auto editor = MakeEditor("a");
  const auto frame = editor.CreateFrame();
  ASSERT_TRUE(editor.HandleKey(Key::MakeText('x')));
  EXPECT_EQ(frame.rows.front(), "  1 a");
  EXPECT_EQ(editor.CreateFrame().rows.front(), "  1 xa");
}

TEST(EditorTest, ExitAndUnrecognizedKeysLeaveStateUnchanged) {
  auto editor = MakeEditor("a");
  ASSERT_TRUE(editor.HandleKey(Key::Special(FunctionKey::F1)));
  ASSERT_TRUE(editor.HandleKey(Key::Special(EditingKey::Escape)));
  ASSERT_FALSE(
      editor.HandleKey(Key::Special(EditingKey::Escape, KeyMods::Alt)));
  EXPECT_EQ(Contents(editor), "a");
  EXPECT_EQ(editor.CursorPosition(), 0U);
  EXPECT_EQ(editor.TopRow(), 0U);
}

TEST(EditorTest, ValidatesViewportDimensionsAndClampsNarrowWidths) {
  EXPECT_THROW(MakeEditor("", 8, 0), std::invalid_argument);
  if constexpr (std::numeric_limits<size_t>::max() >
                std::numeric_limits<ByteCount>::max()) {
    EXPECT_THROW(MakeEditor("", std::numeric_limits<size_t>::max(), 1),
                 std::invalid_argument);
  }
  auto editor = MakeEditor("ab", 0, 1);
  EXPECT_EQ(editor.CreateFrame().text_width, 1U);
  Press(editor, NavigationKey::End);
  EXPECT_EQ(editor.TopRow(), 1U);
  EXPECT_EQ(editor.CreateFrame().cursor_column, 5U);
}

}  // namespace
}  // namespace unplugged
