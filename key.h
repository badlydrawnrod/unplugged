#pragma once

#include <cstdint>
#include <string>

enum class KeyMods : uint8_t {
  None = 0x00,
  Shift = 0x01,
  Alt = 0x02,
  Ctrl = 0x04,
  Super = 0x08,
  Hyper = 0x10,
  Meta = 0x20,
  CapsLock = 0x40,
  ScrollLock = 0x80,
};

constexpr KeyMods operator|(KeyMods lhs, KeyMods rhs) {
  return static_cast<KeyMods>(static_cast<uint8_t>(lhs) |
                              static_cast<uint8_t>(rhs));
}

constexpr KeyMods operator&(KeyMods lhs, KeyMods rhs) {
  return static_cast<KeyMods>(static_cast<uint8_t>(lhs) &
                              static_cast<uint8_t>(rhs));
}

constexpr KeyMods &operator|=(KeyMods &lhs, KeyMods rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class KeyCategory : uint8_t {
  Navigation,
  Editing,
  Function,
  Keypad,
  Misc,
};

enum class NavigationKey : uint16_t {
  Up,
  Down,
  Left,
  Right,
  Home,
  End,
  PageUp,
  PageDown
};
enum class EditingKey : uint16_t {
  Insert,
  Delete,
  Backspace,
  Tab,
  Enter,
  Escape,
  BackTab
};
enum class FunctionKey : uint16_t {
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12
};

enum class KeypadKey : uint16_t {
  KP_0,
  KP_1,
  KP_2,
  KP_3,
  KP_4,
  KP_5,
  KP_6,
  KP_7,
  KP_8,
  KP_9,
  Decimal,
  Divide,
  Multiply,
  Subtract,
  Add,
  Enter,
  Equal,
  Separator,
  Left,
  Right,
  Up,
  Down,
  PageUp,
  PageDown,
  Home,
  End,
  Insert,
  Delete,
};

enum class MiscKey : uint16_t {
  CapsLock,
  ScrollLock,
  NumLock,
  PrintScreen,
  Pause,
  Menu
};

// Factories keep text and typed special-key states mutually exclusive.
// WithMods returns a copy with replacement modifiers and the same identity.
class Key {
 public:
  static constexpr Key MakeText(uint32_t codepoint,
                                KeyMods mods = KeyMods::None) {
    return Key(codepoint, mods);
  }

  static constexpr Key Special(NavigationKey c, KeyMods m = KeyMods::None) {
    return MakeSpecial(KeyCategory::Navigation, static_cast<uint16_t>(c), m);
  }

  static constexpr Key Special(EditingKey c, KeyMods m = KeyMods::None) {
    return MakeSpecial(KeyCategory::Editing, static_cast<uint16_t>(c), m);
  }

  static constexpr Key Special(FunctionKey c, KeyMods m = KeyMods::None) {
    return MakeSpecial(KeyCategory::Function, static_cast<uint16_t>(c), m);
  }

  static constexpr Key Special(KeypadKey c, KeyMods m = KeyMods::None) {
    return MakeSpecial(KeyCategory::Keypad, static_cast<uint16_t>(c), m);
  }

  static constexpr Key Special(MiscKey c, KeyMods m = KeyMods::None) {
    return MakeSpecial(KeyCategory::Misc, static_cast<uint16_t>(c), m);
  }

  constexpr bool IsText() const { return is_text_; }
  constexpr bool IsSpecial() const { return !is_text_; }

  constexpr bool HasMod(KeyMods m) const {
    return (static_cast<uint8_t>(mods_) & static_cast<uint8_t>(m)) ==
           static_cast<uint8_t>(m);
  }

  constexpr bool Is(EditingKey c) const {
    return IsSpecial() && category_ == KeyCategory::Editing &&
           code_ == static_cast<uint16_t>(c);
  }
  constexpr bool Is(EditingKey c, KeyMods m) const {
    return Is(c) && HasMod(m);
  }

  constexpr bool Is(NavigationKey c) const {
    return IsSpecial() && category_ == KeyCategory::Navigation &&
           code_ == static_cast<uint16_t>(c);
  }
  constexpr bool Is(NavigationKey c, KeyMods m) const {
    return Is(c) && HasMod(m);
  }

  constexpr bool Is(FunctionKey c) const {
    return IsSpecial() && category_ == KeyCategory::Function &&
           code_ == static_cast<uint16_t>(c);
  }
  constexpr bool Is(FunctionKey c, KeyMods m) const {
    return Is(c) && HasMod(m);
  }

  constexpr Key WithMods(KeyMods m) const {
    Key k = *this;
    k.mods_ = m;
    return k;
  }

  // Encodes text to UTF-8; returns an empty string for special keys.
  std::string Text() const;

 private:
  constexpr Key(uint32_t codepoint, KeyMods mods)
      : is_text_(true), mods_(mods), codepoint_(codepoint) {}

  constexpr Key(KeyCategory category, uint16_t code, KeyMods mods)
      : is_text_(false), mods_(mods), category_(category), code_(code) {}

  static constexpr Key MakeSpecial(KeyCategory category, uint16_t code,
                                   KeyMods mods) {
    return Key(category, code, mods);
  }

  bool is_text_;
  KeyMods mods_;
  uint32_t codepoint_ = 0;
  KeyCategory category_ = {};
  uint16_t code_ = 0;
};
