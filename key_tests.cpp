#include <gtest/gtest.h>

#include <array>
#include <type_traits>

#include "key.h"

namespace {

// Construction must go through factories; callers cannot supply a discriminator
// and conflicting payloads or an untyped category/code pair.
static_assert(!std::is_aggregate_v<Key>);
static_assert(!std::is_default_constructible_v<Key>);
static_assert(!std::is_constructible_v<Key, uint32_t, KeyMods>);
static_assert(!std::is_constructible_v<Key, KeyCategory, uint16_t, KeyMods>);
static_assert(std::is_copy_constructible_v<Key>);
static_assert(std::is_copy_assignable_v<Key>);

constexpr Key kText = Key::MakeText(0, KeyMods::Ctrl);
static_assert(kText.IsText() && !kText.IsSpecial());
static_assert(!kText.Is(NavigationKey::Up));
static_assert(!kText.Is(EditingKey::Insert));
static_assert(!kText.Is(FunctionKey::F1));
static_assert(kText.HasMod(KeyMods::Ctrl));

constexpr Key kNavigation = Key::Special(NavigationKey::Up);
static_assert(kNavigation.IsSpecial() && !kNavigation.IsText());
static_assert(kNavigation.Is(NavigationKey::Up));
static_assert(!kNavigation.Is(EditingKey::Insert));
static_assert(!kNavigation.Is(FunctionKey::F1));
static_assert(Key::Special(EditingKey::Insert).Is(EditingKey::Insert));
static_assert(Key::Special(FunctionKey::F1).Is(FunctionKey::F1));
static_assert(Key::Special(KeypadKey::KP_0).IsSpecial());
static_assert(Key::Special(MiscKey::CapsLock).IsSpecial());
static_assert(kNavigation.WithMods(KeyMods::Alt | KeyMods::Ctrl)
                  .Is(NavigationKey::Up, KeyMods::Ctrl));
static_assert(!kNavigation.HasMod(KeyMods::Ctrl));

TEST(KeyTest, TextEncodesUtf8AtEncodingBoundaries) {
  struct Example {
    uint32_t codepoint;
    std::string expected;
  };
  const std::array examples = {
      Example{0, std::string(1, '\0')},
      Example{0x7f, "\x7f"},
      Example{0x80, "\xc2\x80"},
      Example{0x7ff, "\xdf\xbf"},
      Example{0x800, "\xe0\xa0\x80"},
      Example{0xd7ff, "\xed\x9f\xbf"},
      Example{0xe000, "\xee\x80\x80"},
      Example{0xffff, "\xef\xbf\xbf"},
      Example{0x10000, "\xf0\x90\x80\x80"},
      Example{0x10ffff, "\xf4\x8f\xbf\xbf"},
  };
  for (const auto& [codepoint, expected] : examples) {
    SCOPED_TRACE(codepoint);
    const Key key = Key::MakeText(codepoint);
    EXPECT_TRUE(key.IsText());
    EXPECT_FALSE(key.IsSpecial());
    EXPECT_EQ(key.Text(), expected);
  }
}

TEST(KeyTest, SpecialCategoriesDoNotAliasOrProduceText) {
  // These enumerators all have the same underlying value in different
  // categories.
  const std::array keys = {
      Key::Special(NavigationKey::Up), Key::Special(EditingKey::Insert),
      Key::Special(FunctionKey::F1),   Key::Special(KeypadKey::KP_0),
      Key::Special(MiscKey::CapsLock),
  };
  for (size_t i = 0; i < keys.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(keys[i].IsSpecial());
    EXPECT_FALSE(keys[i].IsText());
    EXPECT_TRUE(keys[i].Text().empty());
    EXPECT_EQ(keys[i].Is(NavigationKey::Up), i == 0);
    EXPECT_EQ(keys[i].Is(EditingKey::Insert), i == 1);
    EXPECT_EQ(keys[i].Is(FunctionKey::F1), i == 2);
    EXPECT_FALSE(keys[i].Is(NavigationKey::Down));
    EXPECT_FALSE(keys[i].Is(EditingKey::Delete));
    EXPECT_FALSE(keys[i].Is(FunctionKey::F2));
  }
}

TEST(KeyTest, WithModsReplacesModifiersAndPreservesIdentity) {
  const std::array keys = {
      Key::MakeText('x', KeyMods::Shift),
      Key::Special(NavigationKey::Up, KeyMods::Shift),
      Key::Special(EditingKey::Escape, KeyMods::Shift),
      Key::Special(FunctionKey::F1, KeyMods::Shift),
      Key::Special(KeypadKey::Enter, KeyMods::Shift),
      Key::Special(MiscKey::Menu, KeyMods::Shift),
  };
  for (const Key& original : keys) {
    const Key modified = original.WithMods(KeyMods::Alt | KeyMods::Ctrl);
    EXPECT_EQ(modified.IsText(), original.IsText());
    EXPECT_EQ(modified.IsSpecial(), original.IsSpecial());
    EXPECT_EQ(modified.Text(), original.Text());
    EXPECT_EQ(modified.Is(NavigationKey::Up), original.Is(NavigationKey::Up));
    EXPECT_EQ(modified.Is(EditingKey::Escape), original.Is(EditingKey::Escape));
    EXPECT_EQ(modified.Is(FunctionKey::F1), original.Is(FunctionKey::F1));
    EXPECT_TRUE(original.HasMod(KeyMods::Shift));
    EXPECT_FALSE(original.HasMod(KeyMods::Alt));
    EXPECT_FALSE(modified.HasMod(KeyMods::Shift));
    EXPECT_TRUE(modified.HasMod(KeyMods::Alt));
    EXPECT_TRUE(modified.HasMod(KeyMods::Ctrl));
    EXPECT_TRUE(modified.HasMod(KeyMods::Alt | KeyMods::Ctrl));
    EXPECT_FALSE(modified.HasMod(KeyMods::Alt | KeyMods::Shift));
    const Key cleared = modified.WithMods(KeyMods::None);
    EXPECT_FALSE(cleared.HasMod(KeyMods::Alt));
    EXPECT_FALSE(cleared.HasMod(KeyMods::Ctrl));
    EXPECT_TRUE(cleared.HasMod(KeyMods::None));
  }
  EXPECT_TRUE(keys[1]
                  .WithMods(KeyMods::Ctrl | KeyMods::Alt)
                  .Is(NavigationKey::Up, KeyMods::Ctrl));
  EXPECT_TRUE(
      keys[2].WithMods(KeyMods::Alt).Is(EditingKey::Escape, KeyMods::Alt));
  EXPECT_FALSE(
      keys[2].WithMods(KeyMods::Alt).Is(EditingKey::Escape, KeyMods::Ctrl));
  EXPECT_TRUE(
      keys[3].WithMods(KeyMods::Ctrl).Is(FunctionKey::F1, KeyMods::Ctrl));
}

TEST(KeyTest, AssignmentReplacesTheWholeIdentity) {
  Key key = Key::MakeText('x');
  key = Key::Special(EditingKey::Escape, KeyMods::Alt);
  EXPECT_FALSE(key.IsText());
  EXPECT_TRUE(key.IsSpecial());
  EXPECT_TRUE(key.Is(EditingKey::Escape, KeyMods::Alt));
  EXPECT_TRUE(key.Text().empty());

  key = Key::MakeText('y', KeyMods::Ctrl);
  EXPECT_TRUE(key.IsText());
  EXPECT_FALSE(key.IsSpecial());
  EXPECT_FALSE(key.Is(EditingKey::Escape));
  EXPECT_FALSE(key.HasMod(KeyMods::Alt));
  EXPECT_TRUE(key.HasMod(KeyMods::Ctrl));
  EXPECT_EQ(key.Text(), "y");
}

}  // namespace
