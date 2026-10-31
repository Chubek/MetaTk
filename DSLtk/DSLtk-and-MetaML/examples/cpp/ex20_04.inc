metaml::Session s;
auto r = s.type_of("fn x -> x");
assert(r.is_ok());
assert(r.unwrap().find("->") != std::string::npos);
