# Chapter 10 — Byte strings and escaping

## Strings describe bytes

`ascii` accepts one double-quoted byte string. It emits exactly the decoded bytes,
without a terminating NUL, length prefix or character-set conversion.

<!-- verify: target=amd64 emit=hex -->
```mair
ascii "Hello\n\000"
```

The output is `48656c6c6f0a00`. The final zero exists because the source explicitly
contains `\000`, not because `ascii` creates C strings.

## Supported escapes

| Escape | Byte |
| --- | --- |
| `\n` | Newline, 10 |
| `\r` | Carriage return, 13 |
| `\t` | Tab, 9 |
| `\"` | Double quote |
| `\\` | Backslash |
| One to three octal digits after `\` | A value from 0 through 255 |

Unknown escapes, an incomplete escape and an octal value exceeding 255 fail.
There are no `\xNN` or `\uNNNN` escape productions. Use three octal digits when
a following character could otherwise be consumed as another octal digit.
For example, `\0017` is byte 1 followed by the character `7`.

## C++ byte strings

```cpp
mair::Program p;
p.ascii(std::string("A\0B", 3));
auto quoted = mair::quote(std::string("A\0B", 3));
auto original = mair::unquote(quoted);
```

Pass an explicit length when a C++ string contains embedded NULs. `quote` emits
printable ASCII directly, escapes quotes/backslashes and uses three-digit octal
for other bytes. `unquote` reverses the documented escape grammar.

UTF-8 source text is carried as its byte sequence, not as individually interpreted
code points. Text output may show those bytes as octal escapes. Parsing and
rendering therefore preserve the data without promising preservation of its
original source spelling.

[Previous](09-integers.md) · [Contents](README.md) · [Next](11-native-assembly.md)
