dsl::BinaryStream s;
s.write_integer<std::uint16_t>(0x1234, dsl::ByteOrder::Big);
assert(s.bytes.size() == 2);
assert(s.read_integer<std::uint16_t>(dsl::ByteOrder::Big) == 0x1234);
