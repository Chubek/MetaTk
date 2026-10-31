auto s = new MetaML.Session();
string captured;
s.set_output((string x) { captured ~= x; });
s.write("hello");
assert(captured == "hello");
