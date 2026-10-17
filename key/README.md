# Key values

`//key:api` defines the shared [`Key`](include/key/key.h) value type used by
the editor, decoder, and terminal. Construct text keys with `Key::MakeText()`
and typed special keys with `Key::Special()`. A key is either text or one of the
navigation, editing, function, keypad, or miscellaneous categories.

Modifiers are bit flags. `HasMod()` checks that all requested bits are set;
`WithMods()` returns a copy with the modifier set replaced. `Text()` returns
the UTF-8 encoding for a text key and an empty string for a special key. The
[key tests](tests/) cover category separation, modifier behavior, and UTF-8
encoding boundaries.
