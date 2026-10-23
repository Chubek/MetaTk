/**
 * metalib/d/Buffer.d — MetaML host module `Buffer` (D dialect): mutable byte buffers (built on DSLtk BinaryStream).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Buffer.h):
 *   Buffer.create, add, add_line, add_byte, add_u16le, add_u32le, add_u64le,
 *   Buffer.add_u32be, contents, length, byte_at, clear, hex
 */
module metalib.d.Buffer;

import MetaML;
import metalib.d.Support;


import DSLtk : BinaryStream, ByteOrder;

private alias H = Handle!BinaryStream;

private PrimFn writer(int n, ByteOrder order)
{
    return delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[1], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        ulong v = cast(ulong) a[0].i;
        if (n == 2) h.data.write_integer!ushort(cast(ushort) v, order);
        else if (n == 4) h.data.write_integer!uint(cast(uint) v, order);
        else h.data.write_integer!ulong(v, order);
        return unit();
    };
}

private void append(H h, string text)
{
    h.mutex.lock();
    scope (exit) h.mutex.unlock();
    h.data.bytes ~= cast(const(ubyte)[]) text;
}

void installBuffer(Session s)
{
    s.declare("type Buffer\n", "<metalib Buffer>");
    prim(s, "Buffer.create", "Unit -> Buffer", 1, (C c, V a) => wrap_handle(new H("Buffer")));
    prim(s, "Buffer.add", "String -> Buffer -> Unit", 2, delegate Value(C c, V a) {
        append(handle!BinaryStream(a[1], "Buffer", c), S(a[0]));
        return unit();
    });
    prim(s, "Buffer.add_line", "String -> Buffer -> Unit", 2, delegate Value(C c, V a) {
        append(handle!BinaryStream(a[1], "Buffer", c), S(a[0]) ~ "\n");
        return unit();
    });
    prim(s, "Buffer.add_byte", "Int -> Buffer -> Unit", 2, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[1], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        h.data.write_integer!ubyte(cast(ubyte)(a[0].i & 255));
        return unit();
    });
    prim(s, "Buffer.add_u16le", "Int -> Buffer -> Unit", 2, writer(2, ByteOrder.Little));
    prim(s, "Buffer.add_u32le", "Int -> Buffer -> Unit", 2, writer(4, ByteOrder.Little));
    prim(s, "Buffer.add_u64le", "Int -> Buffer -> Unit", 2, writer(8, ByteOrder.Little));
    prim(s, "Buffer.add_u32be", "Int -> Buffer -> Unit", 2, writer(4, ByteOrder.Big));
    prim(s, "Buffer.contents", "Buffer -> String", 1, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[0], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        return str((cast(const(char)[]) h.data.bytes).idup);
    });
    prim(s, "Buffer.length", "Buffer -> Int", 1, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[0], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        return integer(cast(long) h.data.bytes.length);
    });
    prim(s, "Buffer.byte_at", "Int -> Buffer -> Option Int", 2, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[1], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        if (a[0].i < 0 || a[0].i >= cast(long) h.data.bytes.length) return none();
        return some(integer(h.data.bytes[cast(size_t) a[0].i]));
    });
    prim(s, "Buffer.clear", "Buffer -> Unit", 1, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[0], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        h.data.bytes = null;
        h.data.pos = 0;
        return unit();
    });
    prim(s, "Buffer.hex", "Buffer -> String", 1, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[0], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        enum d = "0123456789abcdef";
        string result;
        foreach (x; h.data.bytes)
        {
            result ~= d[x >> 4];
            result ~= d[x & 15];
        }
        return str(result);
    });
    prim(s, "Buffer.of_string", "String -> Buffer", 1, delegate Value(C c, V a) {
        auto h = new H("Buffer");
        append(h, S(a[0]));
        return wrap_handle(h);
    });
    prim(s, "Buffer.truncate", "Int -> Buffer -> Unit", 2, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[1], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        if (a[0].i < 0 || cast(ulong) a[0].i > h.data.bytes.length)
            c.error("M0404", "`Buffer.truncate` length is outside the buffer");
        h.data.bytes.length = cast(size_t) a[0].i;
        if (h.data.pos > h.data.bytes.length) h.data.pos = h.data.bytes.length;
        return unit();
    });
    prim(s, "Buffer.sub", "Int -> Int -> Buffer -> Option String", 3, delegate Value(C c, V a) {
        auto h = handle!BinaryStream(a[2], "Buffer", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        auto n = h.data.bytes.length;
        if (a[0].i < 0 || a[1].i < 0 || cast(ulong) a[0].i > n
            || cast(ulong) a[1].i > n - cast(size_t) a[0].i) return none();
        auto start = cast(size_t) a[0].i;
        return some(str((cast(const(char)[]) h.data.bytes[start .. start + cast(size_t) a[1].i]).idup));
    });
}
