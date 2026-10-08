#include "key_decoder/decoder.h"

#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace unplugged {
namespace {

// Decode a kitty/legacy modifier field. The protocol sends mods+1 (1-based).
// from_chars stops at ':' so the optional ":event-type" suffix is ignored.
KeyMods DecodeMods(std::string_view s) {
  uint8_t mods = 1;
  std::ignore = std::from_chars(s.data(), s.data() + s.size(), mods);
  --mods;
  return static_cast<KeyMods>(mods);
}

// Split the numeric parameters out of a CSI sequence string.
// seq is the full sequence including the \x1b[ prefix and terminator.
// left  = digits before ';' (or the whole number if no ';')
// right = digits after  ';' (defaults to "1" if absent)
struct CsiParams {
  std::string left;
  std::string right;
};

CsiParams SplitCsiParams(const std::string& seq) {
  CsiParams p;
  p.right = "1";
  if (size_t pos = seq.find(';', 2); pos != std::string::npos) {
    p.left = seq.substr(2, pos - 2);
    p.right = seq.substr(pos + 1, seq.size() - 1 - (pos + 1));
  } else {
    p.left = seq.substr(2, seq.size() - 1 - 2);
  }
  return p;
}

// Decode a UTF-8 leading byte plus (n-1) continuation bytes into a codepoint.
uint32_t Utf8Decode(uint8_t leading, const uint8_t* cont, int n) {
  uint32_t cp;
  if (n == 1)
    cp = leading;
  else if (n == 2)
    cp = leading & 0x1f;
  else if (n == 3)
    cp = leading & 0x0f;
  else
    cp = leading & 0x07;
  for (int i = 0; i < n - 1; i++) cp = (cp << 6) | (cont[i] & 0x3f);
  return cp;
}

// Linear scan over small fixed tables — avoids heap allocation.
template <class V, std::size_t N>
const V* TableLookup(const std::array<std::pair<char, V>, N>& table, char key) {
  for (const auto& [k, v] : table)
    if (k == key) return &v;
  return nullptr;
}

template <class V, std::size_t N>
const V* TableLookup(const std::array<std::pair<std::string_view, V>, N>& table,
                     std::string_view key) {
  for (const auto& [k, v] : table)
    if (k == key) return &v;
  return nullptr;
}

constexpr auto kControl = std::to_array<std::pair<char, Key>>({
    {'\x08', Key::Special(EditingKey::Backspace)},  // BS
    {'\x7f', Key::Special(EditingKey::Backspace)},  // DEL
    {'\x09', Key::Special(EditingKey::Tab)},        // HT
    {'\x0d', Key::Special(EditingKey::Enter)},      // CR
});

constexpr auto kSs3 = std::to_array<std::pair<char, Key>>({
    {'P', Key::Special(FunctionKey::F1)},
    {'Q', Key::Special(FunctionKey::F2)},
    {'R', Key::Special(FunctionKey::F3)},
    {'S', Key::Special(FunctionKey::F4)},
    {'A', Key::Special(NavigationKey::Up)},
    {'B', Key::Special(NavigationKey::Down)},
    {'C', Key::Special(NavigationKey::Right)},
    {'D', Key::Special(NavigationKey::Left)},
    {'H', Key::Special(NavigationKey::Home)},
    {'F', Key::Special(NavigationKey::End)},
});

constexpr auto kCsiTilde = std::to_array<std::pair<std::string_view, Key>>({
    {"1", Key::Special(NavigationKey::Home)},
    {"2", Key::Special(EditingKey::Insert)},
    {"3", Key::Special(EditingKey::Delete)},
    {"4", Key::Special(NavigationKey::End)},
    {"5", Key::Special(NavigationKey::PageUp)},
    {"6", Key::Special(NavigationKey::PageDown)},
    {"7", Key::Special(NavigationKey::Home)},
    {"8", Key::Special(NavigationKey::End)},
    {"11", Key::Special(FunctionKey::F1)},
    {"12", Key::Special(FunctionKey::F2)},
    {"13", Key::Special(FunctionKey::F3)},
    {"14", Key::Special(FunctionKey::F4)},
    {"15", Key::Special(FunctionKey::F5)},
    {"17", Key::Special(FunctionKey::F6)},
    {"18", Key::Special(FunctionKey::F7)},
    {"19", Key::Special(FunctionKey::F8)},
    {"20", Key::Special(FunctionKey::F9)},
    {"21", Key::Special(FunctionKey::F10)},
    {"23", Key::Special(FunctionKey::F11)},
    {"24", Key::Special(FunctionKey::F12)},
});

constexpr auto kCsiLetter = std::to_array<std::pair<char, Key>>({
    {'A', Key::Special(NavigationKey::Up)},
    {'B', Key::Special(NavigationKey::Down)},
    {'C', Key::Special(NavigationKey::Right)},
    {'D', Key::Special(NavigationKey::Left)},
    {'H', Key::Special(NavigationKey::Home)},
    {'F', Key::Special(NavigationKey::End)},
    {'Z', Key::Special(EditingKey::BackTab)},
    {'P', Key::Special(FunctionKey::F1)},
    {'Q', Key::Special(FunctionKey::F2)},
    {'R', Key::Special(FunctionKey::F3)},
    {'S', Key::Special(FunctionKey::F4)},
});

constexpr auto kCsiU = std::to_array<std::pair<std::string_view, Key>>({
    {"9", Key::Special(EditingKey::Tab)},
    {"13", Key::Special(EditingKey::Enter)},
    {"27", Key::Special(EditingKey::Escape)},
    {"127", Key::Special(EditingKey::Backspace)},
    {"57358", Key::Special(MiscKey::CapsLock)},
    {"57359", Key::Special(MiscKey::ScrollLock)},
    {"57360", Key::Special(MiscKey::NumLock)},
    {"57361", Key::Special(MiscKey::PrintScreen)},
    {"57362", Key::Special(MiscKey::Pause)},
    {"57363", Key::Special(MiscKey::Menu)},
    {"57399", Key::Special(KeypadKey::KP_0)},
    {"57400", Key::Special(KeypadKey::KP_1)},
    {"57401", Key::Special(KeypadKey::KP_2)},
    {"57402", Key::Special(KeypadKey::KP_3)},
    {"57403", Key::Special(KeypadKey::KP_4)},
    {"57404", Key::Special(KeypadKey::KP_5)},
    {"57405", Key::Special(KeypadKey::KP_6)},
    {"57406", Key::Special(KeypadKey::KP_7)},
    {"57407", Key::Special(KeypadKey::KP_8)},
    {"57408", Key::Special(KeypadKey::KP_9)},
    {"57409", Key::Special(KeypadKey::Decimal)},
    {"57410", Key::Special(KeypadKey::Divide)},
    {"57411", Key::Special(KeypadKey::Multiply)},
    {"57412", Key::Special(KeypadKey::Subtract)},
    {"57413", Key::Special(KeypadKey::Add)},
    {"57414", Key::Special(KeypadKey::Enter)},
    {"57415", Key::Special(KeypadKey::Equal)},
    {"57416", Key::Special(KeypadKey::Separator)},
    {"57417", Key::Special(KeypadKey::Left)},
    {"57418", Key::Special(KeypadKey::Right)},
    {"57419", Key::Special(KeypadKey::Up)},
    {"57420", Key::Special(KeypadKey::Down)},
    {"57421", Key::Special(KeypadKey::PageUp)},
    {"57422", Key::Special(KeypadKey::PageDown)},
    {"57423", Key::Special(KeypadKey::Home)},
    {"57424", Key::Special(KeypadKey::End)},
    {"57425", Key::Special(KeypadKey::Insert)},
    {"57426", Key::Special(KeypadKey::Delete)},
});

}  // namespace

std::optional<Key> DecodeKey(ByteSource& source) {
  const ByteReadResult first = source.ReadByte();
  const auto* first_byte = std::get_if<uint8_t>(&first);
  if (!first_byte) return std::nullopt;

  char ch = static_cast<char>(*first_byte);
  uint8_t byte = static_cast<uint8_t>(ch);
  std::string seq{ch};

  // Not an escape sequence.
  if (ch != '\x1b') {
    // Control character with special meaning?
    if (const Key* it = TableLookup(kControl, ch)) return *it;

    // Ctrl+letter: bytes 0x01–0x1A are Ctrl+A through Ctrl+Z.
    // Normalize to lowercase codepoint + Ctrl to match kitty's form
    // (e.g. 0x01 → 'a' + Ctrl, same as CSI 97 ; 5 u).
    if (byte >= 0x01 && byte <= 0x1a)
      return Key::MakeText(byte + 0x60, KeyMods::Ctrl);

    // Ctrl+\, Ctrl+], Ctrl+^, Ctrl+_ (0x1c–0x1f).
    if (byte >= 0x1c && byte <= 0x1f)
      return Key::MakeText(byte + 0x40, KeyMods::Ctrl);

    // Plain ASCII.
    if ((byte & 0x80) == 0) return Key::MakeText(byte);

    // Multi-byte UTF-8.
    int n = 0;
    if ((byte & 0xf8) == 0xf0)
      n = 4;
    else if ((byte & 0xf0) == 0xe0)
      n = 3;
    else if ((byte & 0xe0) == 0xc0)
      n = 2;

    if (n != 0) {
      uint8_t cont[3]{};
      for (int i = 0; i < n - 1; i++) {
        const ByteReadResult next = source.ReadByte();
        const auto* next_byte = std::get_if<uint8_t>(&next);
        if (!next_byte) return std::nullopt;
        cont[i] = *next_byte;
        if ((cont[i] & 0xc0) != 0x80) return std::nullopt;
      }
      return Key::MakeText(Utf8Decode(byte, cont, n));
    }

    return std::nullopt;
  }

  const ByteReadResult second = source.ReadByte();
  const auto* second_byte = std::get_if<uint8_t>(&second);

  // Nothing further — bare Escape.
  if (!second_byte) {
    if (std::get<ByteReadStatus>(second) == ByteReadStatus::Timeout)
      return Key::Special(EditingKey::Escape);
    return std::nullopt;
  }

  ch = static_cast<char>(*second_byte);
  seq += ch;

  // \x1b\x1b — Alt+Escape.
  if (ch == '\x1b') return Key::Special(EditingKey::Escape, KeyMods::Alt);

  // CSI — \x1b[
  if (ch == '[') {
    ByteReadResult next = source.ReadByte();
    const auto* next_byte = std::get_if<uint8_t>(&next);
    if (!next_byte) return std::nullopt;
    ch = static_cast<char>(*next_byte);
    while (true) {
      seq += ch;
      if (ch >= '@' && ch <= '~') break;
      next = source.ReadByte();
      next_byte = std::get_if<uint8_t>(&next);
      if (!next_byte) return std::nullopt;
      ch = static_cast<char>(*next_byte);
    }

    auto [left, right] = SplitCsiParams(seq);
    KeyMods mods = DecodeMods(right);

    if (ch == '~') {
      if (const Key* it = TableLookup(kCsiTilde, left))
        return it->WithMods(mods);
    } else if (ch == 'u') {
      if (const Key* it = TableLookup(kCsiU, left)) return it->WithMods(mods);
      // Fallback: codepoint (e.g. CSI 97 ; 3 u = Alt+a).
      uint32_t cp = 0;
      if (auto [ptr, ec] =
              std::from_chars(left.data(), left.data() + left.size(), cp);
          ec == std::errc{})
        return Key::MakeText(cp, mods);
    } else if (ch >= '@' && ch < '~') {
      if (const Key* it = TableLookup(kCsiLetter, ch))
        return it->WithMods(mods);
    }

    return std::nullopt;
  }

  // SS3 — \x1bO
  if (ch == 'O') {
    ByteReadResult next = source.ReadByte();
    const auto* next_byte = std::get_if<uint8_t>(&next);
    if (!next_byte) return std::nullopt;
    ch = static_cast<char>(*next_byte);
    if (const Key* it = TableLookup(kSs3, ch)) return *it;
    return std::nullopt;
  }

  // Alt+control fallback (\x1b\x7f = Alt+Backspace, \x1b\x09 = Alt+Tab, etc.).
  if (const Key* it = TableLookup(kControl, ch))
    return it->WithMods(KeyMods::Alt);

  // Alt+char fallback (\x1b + printable ASCII or 2–3 byte UTF-8).
  {
    uint8_t alt_byte = static_cast<uint8_t>(ch);
    int n = 0;
    if (alt_byte >= 0x20 && alt_byte <= 0x7e)
      n = 1;
    else if ((alt_byte & 0xf0) == 0xe0)
      n = 3;
    else if ((alt_byte & 0xe0) == 0xc0)
      n = 2;

    if (n != 0) {
      // Normalize: uppercase → lowercase + Shift to match kitty's form
      // (e.g. \x1b A → 'a' + Shift+Alt, same as CSI 97 ; 4 u).
      KeyMods alt_mods = KeyMods::Alt;
      if (n == 1 && alt_byte >= 'A' && alt_byte <= 'Z') {
        alt_byte += 0x20;
        alt_mods |= KeyMods::Shift;
      }

      uint8_t cont[2]{};
      for (int i = 0; i < n - 1; i++) {
        const ByteReadResult next = source.ReadByte();
        const auto* next_byte = std::get_if<uint8_t>(&next);
        if (!next_byte) return std::nullopt;
        cont[i] = *next_byte;
        if ((cont[i] & 0xc0) != 0x80) return std::nullopt;
      }
      return Key::MakeText(Utf8Decode(alt_byte, cont, n), alt_mods);
    }
  }

  return std::nullopt;
}

}  // namespace unplugged
