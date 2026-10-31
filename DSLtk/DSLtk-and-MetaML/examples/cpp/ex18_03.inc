metaml::Session s;
auto r = s.eval("module N := struct\nval answer := 42\nend\nopen N\nanswer");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
