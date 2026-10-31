BinaryStream s;
s.write_integer!uint(7);
auto bytes = s.read(4);
assert(bytes.length == 4);
assert(s.eof());
