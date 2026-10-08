#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "key_decoder/decoder.h"
#include "key_decoder/ports/byte_source/conformance.h"

namespace unplugged {
namespace {

class Stream final : public ByteSource {
 public:
  explicit Stream(std::string_view bytes,
                  ByteReadStatus end = ByteReadStatus::Eof)
      : end_(end) {
    for (unsigned char byte : bytes) events_.emplace_back(uint8_t{byte});
  }
  Stream(std::initializer_list<ByteReadResult> events) : events_(events) {}

  ByteReadResult ReadByte() override {
    if (position_ == events_.size()) return end_;
    return events_[position_++];
  }

 private:
  std::vector<ByteReadResult> events_;
  size_t position_ = 0;
  ByteReadStatus end_ = ByteReadStatus::Eof;
};

// Feature: key_decoder/ports/byte_source/features/acquisition.feature
// Scenario: Finite input preserves every byte before EOF
TEST(StreamConformanceTest, PreservesAllBytesThenReportsEof) {
  const auto bytes = byte_source_conformance::AllBytes();
  Stream stream(std::string_view(reinterpret_cast<const char*>(bytes.data()),
                                 bytes.size()));
  byte_source_conformance::ExpectOrderedBytes(stream, bytes);
  byte_source_conformance::ExpectEof(stream);
}

// Feature: key_decoder/ports/byte_source/features/acquisition.feature
// Scenario: Input can continue after a timeout
TEST(StreamConformanceTest, TimeoutDoesNotConsumeFollowingBytes) {
  Stream stream{ByteReadStatus::Timeout, uint8_t{0}, uint8_t{255},
                uint8_t{'x'}};
  byte_source_conformance::ExpectTimeout(stream);
  byte_source_conformance::ExpectOrderedBytes(
      stream, std::array<uint8_t, 3>{0, 255, 'x'});
  byte_source_conformance::ExpectEof(stream);
}

TEST(StreamConformanceTest, ErrorIsDistinctFromBytesAndDoesNotConsumeThem) {
  Stream stream{ByteReadStatus::Error, uint8_t{0}, uint8_t{255}};
  EXPECT_EQ(stream.ReadByte(), ByteReadResult{ByteReadStatus::Error});
  byte_source_conformance::ExpectOrderedBytes(stream,
                                              std::array<uint8_t, 2>{0, 255});
  byte_source_conformance::ExpectEof(stream);
}

void ExpectMods(const Key& key, KeyMods mods) {
  for (auto bit : {KeyMods::Shift, KeyMods::Alt, KeyMods::Ctrl, KeyMods::Super,
                   KeyMods::Hyper, KeyMods::Meta, KeyMods::CapsLock,
                   KeyMods::ScrollLock}) {
    EXPECT_EQ(key.HasMod(bit), (mods & bit) == bit);
  }
}

void ExpectText(const std::optional<Key>& key, std::string_view text,
                KeyMods mods = KeyMods::None) {
  ASSERT_TRUE(key);
  EXPECT_TRUE(key->IsText());
  EXPECT_FALSE(key->IsSpecial());
  EXPECT_EQ(key->Text(), text);
  ExpectMods(*key, mods);
}

template <class Special>
void ExpectSpecial(const std::optional<Key>& key, Special special,
                   KeyMods mods = KeyMods::None) {
  ASSERT_TRUE(key);
  EXPECT_TRUE(key->IsSpecial());
  EXPECT_TRUE(key->Is(special));
  EXPECT_TRUE(key->Text().empty());
  ExpectMods(*key, mods);
}

TEST(DecoderTest, DecodesAsciiAndNulWithoutConsumingFollowingText) {
  Stream stream(std::string_view("x\0y", 3));
  ExpectText(DecodeKey(stream), "x");
  ExpectText(DecodeKey(stream), std::string_view("\0", 1));
  ExpectText(DecodeKey(stream), "y");
  EXPECT_FALSE(DecodeKey(stream));
}

TEST(DecoderTest, NormalizesLegacyControlLettersAndPunctuation) {
  struct Example {
    char byte;
    std::string_view text;
  };
  const std::array examples = {
      Example{'\x01', "a"}, Example{'\x02', "b"},  Example{'\x0a', "j"},
      Example{'\x1a', "z"}, Example{'\x1c', "\\"}, Example{'\x1d', "]"},
      Example{'\x1e', "^"}, Example{'\x1f', "_"},
  };
  for (const auto& [byte, text] : examples) {
    SCOPED_TRACE(text);
    Stream stream(std::string(1, byte));
    ExpectText(DecodeKey(stream), text, KeyMods::Ctrl);
  }
}

TEST(DecoderTest, DecodesLegacyEditingControlsAndAltControls) {
  struct Example {
    char byte;
    EditingKey key;
  };
  const std::array examples = {
      Example{'\x08', EditingKey::Backspace},
      Example{'\x7f', EditingKey::Backspace},
      Example{'\t', EditingKey::Tab},
      Example{'\r', EditingKey::Enter},
  };
  for (const auto& [byte, key] : examples) {
    Stream plain(std::string(1, byte));
    ExpectSpecial(DecodeKey(plain), key);
    Stream alt(std::string("\x1b") + byte);
    ExpectSpecial(DecodeKey(alt), key, KeyMods::Alt);
  }
}

// Feature: key_decoder/features/decoding.feature
// Scenario: UTF-8 input produces one text key per character
TEST(DecoderTest, DecodesMultibyteUtf8WithoutConsumingTheNextCharacter) {
  Stream stream("\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80x");
  ExpectText(DecodeKey(stream), "\xc3\xa9");
  ExpectText(DecodeKey(stream), "\xe2\x82\xac");
  ExpectText(DecodeKey(stream), "\xf0\x9f\x98\x80");
  ExpectText(DecodeKey(stream), "x");
  EXPECT_FALSE(DecodeKey(stream));
}

TEST(DecoderTest, NormalizesAltAsciiAndDecodesAltUtf8) {
  struct Example {
    std::string_view bytes;
    std::string_view text;
    KeyMods mods;
  };
  const std::array examples = {
      Example{"\x1b"
              "a",
              "a", KeyMods::Alt},
      Example{"\x1b"
              "A",
              "a", KeyMods::Alt | KeyMods::Shift},
      Example{"\x1b!", "!", KeyMods::Alt},
      Example{"\x1b\xc3\xa9", "\xc3\xa9", KeyMods::Alt},
      Example{"\x1b\xe2\x82\xac", "\xe2\x82\xac", KeyMods::Alt},
  };
  for (const auto& [bytes, text, mods] : examples) {
    SCOPED_TRACE(text);
    Stream stream(bytes);
    ExpectText(DecodeKey(stream), text, mods);
  }
}

TEST(DecoderTest, DecodesCsiAndSs3NavigationKeys) {
  struct Example {
    std::string_view bytes;
    NavigationKey key;
    KeyMods mods;
  };
  const std::array examples = {
      Example{"\x1b[A", NavigationKey::Up, KeyMods::None},
      Example{"\x1b[B", NavigationKey::Down, KeyMods::None},
      Example{"\x1b[C", NavigationKey::Right, KeyMods::None},
      Example{"\x1b[D", NavigationKey::Left, KeyMods::None},
      Example{"\x1b[H", NavigationKey::Home, KeyMods::None},
      Example{"\x1b[F", NavigationKey::End, KeyMods::None},
      Example{"\x1b[5~", NavigationKey::PageUp, KeyMods::None},
      Example{"\x1b[6~", NavigationKey::PageDown, KeyMods::None},
      Example{"\x1b[1~", NavigationKey::Home, KeyMods::None},
      Example{"\x1b[7~", NavigationKey::Home, KeyMods::None},
      Example{"\x1b[4~", NavigationKey::End, KeyMods::None},
      Example{"\x1b[8~", NavigationKey::End, KeyMods::None},
      Example{"\x1b[1;5A", NavigationKey::Up, KeyMods::Ctrl},
      Example{"\x1b[6;3~", NavigationKey::PageDown, KeyMods::Alt},
      Example{"\x1bOA", NavigationKey::Up, KeyMods::None},
      Example{"\x1bOB", NavigationKey::Down, KeyMods::None},
      Example{"\x1bOC", NavigationKey::Right, KeyMods::None},
      Example{"\x1bOD", NavigationKey::Left, KeyMods::None},
      Example{"\x1bOH", NavigationKey::Home, KeyMods::None},
      Example{"\x1bOF", NavigationKey::End, KeyMods::None},
  };
  for (const auto& [bytes, key, mods] : examples) {
    SCOPED_TRACE(bytes);
    Stream stream(bytes);
    ExpectSpecial(DecodeKey(stream), key, mods);
  }
}

TEST(DecoderTest, DecodesFunctionKeysAcrossSupportedSequenceForms) {
  struct Example {
    std::string_view bytes;
    FunctionKey key;
  };
  const std::array examples = {
      Example{"\x1b[11~", FunctionKey::F1},
      Example{"\x1b[12~", FunctionKey::F2},
      Example{"\x1b[13~", FunctionKey::F3},
      Example{"\x1b[14~", FunctionKey::F4},
      Example{"\x1b[15~", FunctionKey::F5},
      Example{"\x1b[17~", FunctionKey::F6},
      Example{"\x1b[18~", FunctionKey::F7},
      Example{"\x1b[19~", FunctionKey::F8},
      Example{"\x1b[20~", FunctionKey::F9},
      Example{"\x1b[21~", FunctionKey::F10},
      Example{"\x1b[23~", FunctionKey::F11},
      Example{"\x1b[24~", FunctionKey::F12},
      Example{"\x1bOP", FunctionKey::F1},
      Example{"\x1bOQ", FunctionKey::F2},
      Example{"\x1bOR", FunctionKey::F3},
      Example{"\x1bOS", FunctionKey::F4},
      Example{"\x1b[P", FunctionKey::F1},
      Example{"\x1b[Q", FunctionKey::F2},
      Example{"\x1b[R", FunctionKey::F3},
      Example{"\x1b[S", FunctionKey::F4},
  };
  for (const auto& [bytes, key] : examples) {
    SCOPED_TRACE(bytes);
    Stream stream(bytes);
    ExpectSpecial(DecodeKey(stream), key);
  }
}

TEST(DecoderTest, DecodesCsiAndKittyEditingKeys) {
  struct Example {
    std::string_view bytes;
    EditingKey key;
  };
  const std::array examples = {
      Example{"\x1b[2~", EditingKey::Insert},
      Example{"\x1b[3~", EditingKey::Delete},
      Example{"\x1b[Z", EditingKey::BackTab},
      Example{"\x1b[9u", EditingKey::Tab},
      Example{"\x1b[13u", EditingKey::Enter},
      Example{"\x1b[27u", EditingKey::Escape},
      Example{"\x1b[127u", EditingKey::Backspace},
  };
  for (const auto& [bytes, key] : examples) {
    Stream stream(bytes);
    ExpectSpecial(DecodeKey(stream), key);
  }
  Stream modified("\x1b[27;3u");
  ExpectSpecial(DecodeKey(modified), EditingKey::Escape, KeyMods::Alt);
}

// Feature: key_decoder/features/decoding.feature
// Scenario: Legacy and kitty control input decode to the same key
TEST(DecoderTest, LegacyAndKittyControlInputAreEquivalent) {
  Stream legacy("\x01");
  Stream kitty("\x1b[97;5u");
  ExpectText(DecodeKey(legacy), "a", KeyMods::Ctrl);
  ExpectText(DecodeKey(kitty), "a", KeyMods::Ctrl);
}

TEST(DecoderTest, DecodesKittyCodepointsModifiersAndEventSuffix) {
  Stream shifted_alt("\x1b[97;4u");
  ExpectText(DecodeKey(shifted_alt), "a", KeyMods::Shift | KeyMods::Alt);
  Stream unicode("\x1b[128512;3u");
  ExpectText(DecodeKey(unicode), "\xf0\x9f\x98\x80", KeyMods::Alt);
  Stream event("\x1b[97;5:2u");
  ExpectText(DecodeKey(event), "a", KeyMods::Ctrl);
  Stream high_modifiers("\x1b[97;255u");
  ExpectText(DecodeKey(high_modifiers), "a", static_cast<KeyMods>(0xfe));
  for (std::string_view bytes : {"\x1b[57358;3u", "\x1b[57414;3u"}) {
    Stream special(bytes);
    const auto key = DecodeKey(special);
    ASSERT_TRUE(key);
    EXPECT_TRUE(key->IsSpecial());
    EXPECT_TRUE(key->Text().empty());
    ExpectMods(*key, KeyMods::Alt);
  }
}

// Feature: key_decoder/features/decoding.feature
// Scenario: Escape is distinguished by the next byte or a timeout
TEST(DecoderTest, EscapeRequiresTimeoutOrFollowingByteToIdentifyTheKey) {
  Stream bare("\x1b", ByteReadStatus::Timeout);
  ExpectSpecial(DecodeKey(bare), EditingKey::Escape);
  Stream alt_escape("\x1b\x1b");
  ExpectSpecial(DecodeKey(alt_escape), EditingKey::Escape, KeyMods::Alt);
  Stream alt_text(
      "\x1b"
      "a");
  ExpectText(DecodeKey(alt_text), "a", KeyMods::Alt);
  for (auto status : {ByteReadStatus::Eof, ByteReadStatus::Error}) {
    Stream interrupted("\x1b", status);
    EXPECT_FALSE(DecodeKey(interrupted));
  }
}

TEST(DecoderTest, InitialNonByteOutcomesProduceNoKey) {
  for (auto status :
       {ByteReadStatus::Timeout, ByteReadStatus::Eof, ByteReadStatus::Error}) {
    Stream stream("", status);
    EXPECT_FALSE(DecodeKey(stream));
  }
}

TEST(DecoderTest, InterruptedSequencesProduceNoPartialKey) {
  for (std::string_view prefix :
       {"\x1b[", "\x1b[1;", "\x1bO", "\xc3", "\xe2\x82", "\xf0\x9f\x98",
        "\x1b\xc3", "\x1b\xe2\x82"}) {
    for (auto status : {ByteReadStatus::Timeout, ByteReadStatus::Eof,
                        ByteReadStatus::Error}) {
      Stream stream(prefix, status);
      EXPECT_FALSE(DecodeKey(stream));
    }
  }
}

TEST(DecoderTest, UnsupportedAndMalformedInputDoesNotConsumeNextKey) {
  for (std::string_view bytes : {"\x1b[99~x", "\x1bOZx", "\x80x", "\xc3!x",
                                 "\x1b[999999999999999ux", "\x1b\xc3!x"}) {
    Stream stream(bytes);
    EXPECT_FALSE(DecodeKey(stream));
    ExpectText(DecodeKey(stream), "x");
  }
}

TEST(DecoderTest, DiscardsInterruptedPrefixBeforeDecodingLaterInput) {
  Stream stream{uint8_t{0x1b}, uint8_t{'['}, ByteReadStatus::Timeout,
                uint8_t{'x'}};
  EXPECT_FALSE(DecodeKey(stream));
  ExpectText(DecodeKey(stream), "x");
}

}  // namespace
}  // namespace unplugged
