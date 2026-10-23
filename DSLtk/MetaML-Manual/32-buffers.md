# 32. Buffer: mutable binary assembly

Buffer is an opaque mutable host handle backed by DSLtk's binary stream. Install its host module and allow `Buffer.*` before calling it from an embedding. The portable host bundle includes the installer; it does not grant permission automatically.

```metaml
b := Buffer.of_string "A\0"
Buffer.add_u16le 258 b
Test.equal "binary bytes" (Buffer.hex b) "41000201"
Test.equal "checked slice" (Buffer.sub 1 2 b) (Some "\0\u{02}")
Buffer.truncate 2 b
Buffer.add "Z" b
Test.equal "truncate then append" (Buffer.contents b) "A\0Z"
```

`create ()` starts empty; `of_string bytes` makes an independent buffer. `add`, `add_line`, and `add_byte` append data. Integer writers include little-endian 16/32/64-bit and big-endian 32-bit forms. Writers retain the requested low bits; they are not checked signed-number encoders.

`contents` returns a snapshot byte string, `length` a byte count, `byte_at` an optional byte, and `hex` lowercase hexadecimal. `sub start length buffer` returns None for negative or out-of-range bounds. Its bounds check avoids adding two potentially huge indices. An empty slice at the end is valid.

`truncate length buffer` only shrinks or preserves the current size. Negative lengths and attempts to grow produce `M0404`; it does not silently pad with zeros. `clear` empties the buffer and resets stream position.

Operations lock the individual handle, so one operation does not race another operation on that same handle. This does not make a sequence of several operations atomic, nor does it make arbitrary Session access thread-safe. Snapshots do not track future mutation. Sharing the handle shares storage; copying a MetaML Value is not a deep clone.

**Sources:** [`cpp/Buffer.h`](../metalib/cpp/Buffer.h), [`d/Buffer.d`](../metalib/d/Buffer.d).

<!-- nav:begin -->
[Contents](README.md) · [Previous](31-input-output.md) · [Next](33-tables.md)
<!-- nav:end -->
