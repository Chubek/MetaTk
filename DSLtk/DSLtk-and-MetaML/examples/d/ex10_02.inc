BinaryStream s;
s.write_integer!ushort(0x1234, ByteOrder.Big);
assert(s.bytes.length == 2);
assert(s.read_integer!ushort(ByteOrder.Big) == 0x1234);
