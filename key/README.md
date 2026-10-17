# Key values

## Create a key

Depend on `//key:api` and include [`Key`](include/key/key.h), the shared value
type used by the editor, decoder, and terminal. Construct text keys with
`Key::MakeText()` and typed special keys with `Key::Special()`. A key is either
text or belongs to a navigation, editing, function, keypad, or miscellaneous
category.

## Read text and modifiers

Modifiers are bit flags. `HasMod()` checks that all requested bits are set;
`WithMods()` returns a copy with the modifier set replaced. `Text()` returns
the UTF-8 encoding for a text key and an empty string for a special key. The
[key tests](tests/) cover category separation, modifier behavior, and UTF-8
encoding boundaries. The API and tests do not currently define the valid input
range for `Key::MakeText()`.
