# Chapter 33 — User Codecs, Abstract I/O and Static Binary Streams

The user defines data syntax and backend operations. DSLtk provides protocols,
template forwarding, byte-stream helpers and static byte matching.

## Serializer and deserializer protocols

`Serializer<Backend>` / `Serializer!Backend` borrows a backend with
`serialize(value, output)`. `Deserializer` borrows one with
`deserialize<T>(input)` in C++ or `deserialize!T(input)` in D. Input/output
types and operation results are chosen by the backend; they need not be I/O
adapters or binary streams.

The following user codec chooses a four-byte, big-endian integer syntax.

```cpp
#include "DSLtk.hpp"
#include <cassert>

struct IntegerCodec {
    void serialize(const int& value, dsl::BinaryStream& output) {
        output.write_integer(static_cast<std::uint32_t>(value), dsl::ByteOrder::Big);
    }
    template<class T> T deserialize(dsl::BinaryStream& input) {
        return static_cast<T>(input.read_integer<std::uint32_t>(dsl::ByteOrder::Big));
    }
};

int main() {
    IntegerCodec codec;
    dsl::Serializer serializer(codec);
    dsl::Deserializer deserializer(codec);
    dsl::BinaryStream encoded;
    serializer.serialize(42, encoded);
    assert(deserializer.deserialize<int>(encoded) == 42);
}
```

```d
struct IntegerCodec
{
    void serialize(int value, ref BinaryStream output)
    { output.write_integer(cast(uint) value, ByteOrder.Big); }
    T deserialize(T)(ref BinaryStream input)
    { return cast(T) input.read_integer!uint(ByteOrder.Big); }
}

IntegerCodec codec;
auto serializer = Serializer!IntegerCodec(codec);
auto deserializer = Deserializer!IntegerCodec(codec);
BinaryStream encoded;
serializer.serialize(42, encoded);
assert(deserializer.deserialize!int(encoded) == 42);
```

C++ protocol queries are `SerializerBackend<B, T, Output>` and
`DeserializerBackend<B, T, Input>` concepts. D uses the corresponding
`SerializerBackend!(B, T, Output)` and `DeserializerBackend!(B, T, Input)`
templates. A backend can return its own result/error object; forwarding
preserves the return type. The adapter does not add a wire format or schema.

## Abstract byte I/O

The `IOBackend` protocol requires both byte-read and byte-write operations:

| Operation | C++ buffer | D buffer | Result |
|---|---|---|---|
| `read` | `std::span<std::byte>` | `ubyte[]` | Transferred byte count |
| `write` | `std::span<const std::byte>` | `const(ubyte)[]` | Transferred byte count |

`IO` borrows its backend. In C++ it stores a reference; in D the constructor
takes a `ref` backend and stores a pointer. Keep the backend alive and at a
stable location for the adapter's use. The backend owns its resource policy.

Here is a user-defined sequential memory backend in D:

```d
struct MemoryBackend
{
    ubyte[] data;
    size_t pos;
    size_t read(ubyte[] target)
    {
        auto available = data.length - pos;
        auto n = target.length < available ? target.length : available;
        target[0 .. n] = data[pos .. pos + n];
        pos += n;
        return n;
    }
    size_t write(const(ubyte)[] source)
    { data ~= source; return source.length; }
}

MemoryBackend backend;
auto io = IO!MemoryBackend(backend);
ubyte[] payload = [0, 255, 65];
io.write_all(payload);
auto stream = BinaryStream.load(io);
assert(stream.bytes == payload);
```

The C++ form substitutes a vector, spans and `IO io(backend)`. A user can
equally supply a file, socket, device or callback backend with this protocol.

### Transfers and optional operations

`read`/`write` validate that a backend's count does not exceed its buffer.
Short transfers are permitted. Zero read denotes EOF. `read_exact` fills an
entire buffer or raises an exception on premature EOF; `write_all` completes
all output or raises an exception when a write makes no progress. Backend
exceptions propagate.

`seek(position)` and `tell()` are available when the backend implements them.
The core read/write protocol does not require random access. The example above
therefore has neither operation.

## Static byte regular expressions

`StaticRegex<Source>` in C++ and `StaticRegex!Source` in D compile a Thompson
NFA from a string template argument. Invalid supported-syntax patterns are
diagnosed at compilation. Matching uses byte values, so NUL, newlines and
bytes from `0x80` through `0xff` are ordinary input values.

| Syntax | Meaning |
|---|---|
| `a`, `\*`, `\(` | Literal/escaped literal byte |
| `.` | Any byte |
| `[abc]`, `[a-z]`, `[^a-z]` | Class, range, negated class |
| `(ab|cd)` | Grouping and alternation |
| `a*`, `a+`, `a?` | Zero-or-more, one-or-more, optional |
| `^`, `$` | Absolute beginning/end of the complete input |
| `\xHH`, `\0` | Hexadecimal byte and NUL |
| `\n`, `\r`, `\t` | Newline, carriage return, tab |

The current engine does not supply captures, counted repetitions,
backreferences or shorthand classes such as `\d`. Use `[0-9]` for digits.

```cpp
assert(dsl::StaticRegex<"^(ab|a)+[0-9]?$">::matches("aba7"));
std::vector<std::byte> bytes{std::byte{0}, std::byte{255}, std::byte{'A'}};
assert(dsl::StaticRegex<"\\x00[\\x80-\\xff]A">::matches(bytes));
```

```d
assert(StaticRegex!"^(ab|a)+[0-9]?$".matches("aba7"));
ubyte[] bytes = [0, 255, 65];
assert(StaticRegex!`\x00[\x80-\xff]A`.matches(bytes));
```

`matches` requires the entire input. `prefix(bytes, start)` returns an optional
longest matching end offset. `search(bytes, start)` returns the earliest
matching `RegexMatch`, choosing its longest end at that start. Match records
have `begin`, `end`, and `size()`. C++ uses `std::optional`; D uses `Nullable`.
Absolute `^`/`$` assertions still refer to the full input when `start` is nonzero.

The existing `pattern` and PEG `CompiledPattern` APIs have their own smaller
syntax and matching semantics. Static byte regexes provide the grouping,
alternation and binary-stream matching facility documented here.

## Binary stream operations

`BinaryStream` owns `bytes` and a read cursor `pos`. C++ accepts an input span;
D accepts a `const(ubyte)[]`; each constructor copies the input. `read(count)`
returns a const view and advances the cursor, raising an exception on EOF.
`write(input)` appends to storage rather than writing at the cursor.

```cpp
dsl::BinaryStream stream(bytes);
auto token = stream.next<"\\0[\\x80-\\xff]">();
assert(token && token->size() == 2);
assert(stream.pos == 2);
```

```d
auto stream = BinaryStream(bytes);
auto token = stream.next!`\0[\x80-\xff]`();
assert(token.get.length == 2 && stream.pos == 2);
```

`next` runs a static regex at the current cursor. Failure returns an empty
optional and leaves `pos` unchanged. A successful zero-length token raises an
exception. Returned views refer to the stream's storage; storage-changing
operations can invalidate views in C++. D slices retain their referenced
allocation, but callers should obtain fresh views after changing storage.

Unsigned `read_integer<T>` / `read_integer!T` and `write_integer(value, order)`
use `ByteOrder::Little/Big` in C++ or `ByteOrder.Little/Big` in D. The default
is little-endian; each operation consumes/appends the type's byte width.

`BinaryStream.load(io, limit)` repeatedly reads through abstract I/O until EOF.
Its default limit is 64 MiB. Exceeding the configured limit raises an exception.
`save(io)` writes the whole buffer through `write_all`, regardless of `pos`.
The user codec can compose these operations into its own file/record protocol.
