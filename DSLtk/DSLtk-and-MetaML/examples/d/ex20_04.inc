auto s = new MetaML.Session();
auto r = s.type_of("fn x -> x");
assert(r.is_ok());
import std.string : indexOf;
assert(r.unwrap().indexOf("->") >= 0);
