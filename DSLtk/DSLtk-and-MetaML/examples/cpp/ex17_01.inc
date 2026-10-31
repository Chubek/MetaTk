metaml::Session s;
auto r = s.eval("match Some 42 with | Some x -> x | None -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
