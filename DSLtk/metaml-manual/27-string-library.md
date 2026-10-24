# 27. String: bytes, text, and UTF-8

The portable String API works on immutable bytes. Lengths and indices count bytes, `chars` returns one-byte strings, and `reverse` reverses bytes. This is appropriate for binary protocols and ASCII, but it can split a multibyte UTF-8 sequence. Unicode scalar escapes are encoded as UTF-8 when lexed; that does not change the indexing model.

```metaml
Test.equal "byte length" (String.length "\u{e9}") 2
Test.equal "split preserves empty parts" (String.split "," "a,,b,") ["a", "", "b", ""]
Test.equal "clamped substring" (String.sub 1 99 "abcd") "bcd"
Test.equal "safe get" (String.get 99 "abcd") None
Test.equal "replacement" (String.replace "aa" "x" "aaaa") "xx"
```

`sub start length text` clamps to the valid interval; `get index text` returns Option. `index_of needle haystack` returns a byte offset. `split "" text` splits bytes, while a nonempty separator preserves empty fields. `replace` handles non-overlapping matches and leaves the input unchanged for an empty search string.

`repeat count text` returns empty for non-positive counts or empty input. Oversized nonempty results raise `M0404`, using a division-based bound check that cannot overflow. `upper`, `lower`, and the core trimming behavior are ASCII-oriented, not locale-aware Unicode case folding.

The installed String host module adds `utf8_valid`, `utf8_length`, `utf8_chars`, `codepoints`, and `of_codepoints`, plus padding, word splitting, formatting, and classification helpers. Use `utf8_valid` before treating arbitrary bytes as Unicode. Code points are not grapheme clusters: combining sequences and emoji can occupy several scalars.

```metaml
Test.equal "Unicode scalars" (String.utf8_chars "a\u{e9}") ["a", "\u{e9}"]
Test.check "UTF-8 validation" (not (String.utf8_valid (String.of_codes [255])))
```

**Sources:** [`string/mod.ml`](../metalib/string/mod.ml), [`cpp/String.h`](../metalib/cpp/String.h).

<!-- api:begin -->
```text
  type t = String
  val empty : t
  val length : t -> Int
  val is_empty : t -> Bool
  val concat : t -> t -> t
  val sub : Int -> Int -> t -> t
  val get : Int -> t -> Option t
  val index_of : t -> t -> Option Int
  val chars : t -> [t]
  val join : t -> [t] -> t
  val split : t -> t -> [t]
  val lines : t -> [t]
  val trim : t -> t
  val upper : t -> t
  val lower : t -> t
  val starts_with : t -> t -> Bool
  val ends_with : t -> t -> Bool
  val contains : t -> t -> Bool
  val replace : t -> t -> t -> t
  val repeat : Int -> t -> t
  val reverse : t -> t
  val compare : t -> t -> Int
  val equal : t -> t -> Bool
  val to_codes : t -> [Int]
  val of_codes : [Int] -> t
  val of_int : Int -> t
  val of_float : Float -> t
  val to_int : t -> Option Int
```
<!-- api:end -->

<!-- nav:begin -->
[Contents](README.md) · [Previous](26-floating-library.md) · [Next](28-list-library.md)
<!-- nav:end -->
