metaml::Session s;
auto r = s.eval("module Policy := struct\nval allow := fn age -> age >= 18\nend\nPolicy.allow 20");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "true");
