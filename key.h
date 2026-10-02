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

struct Key {
  bool isText = false;
  KeyMods mods = KeyMods::None;
  uint32_t codepoint = 0;     // valid when isText == true
  KeyCategory category = {};  // valid when isText == false
  uint16_t code = 0;          // valid when isText == false

  static constexpr Key MakeText(uint32_t cp, KeyMods m = KeyMods::None) {
    Key k;
    k.isText = true;
    k.codepoint = cp;
    k.mods = m;
    return k;
  }

  static constexpr Key MakeSpecial(KeyCategory cat, uint16_t c, KeyMods m) {
    Key k;
    k.isText = false;
    k.category = cat;
    k.code = c;
    k.mods = m;
    return k;
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

  bool IsText() const { return isText; }
  bool IsSpecial() const { return !isText; }

  bool HasMod(KeyMods m) const {
    return (static_cast<uint8_t>(mods) & static_cast<uint8_t>(m)) ==
           static_cast<uint8_t>(m);
  }

  bool Is(EditingKey c) const {
    return IsSpecial() && category == KeyCategory::Editing &&
           code == static_cast<uint16_t>(c);
  }
  bool Is(EditingKey c, KeyMods m) const { return Is(c) && HasMod(m); }

  bool Is(NavigationKey c) const {
    return IsSpecial() && category == KeyCategory::Navigation &&
           code == static_cast<uint16_t>(c);
  }
  bool Is(NavigationKey c, KeyMods m) const { return Is(c) && HasMod(m); }

  bool Is(FunctionKey c) const {
    return IsSpecial() && category == KeyCategory::Function &&
           code == static_cast<uint16_t>(c);
  }
  bool Is(FunctionKey c, KeyMods m) const { return Is(c) && HasMod(m); }

  Key WithMods(KeyMods m) const {
    Key k = *this;
    k.mods = m;
    return k;
  }

  // Encodes the codepoint to UTF-8. Only valid when IsText() == true.
  std::string Text() const;
};
