metaml::Session s;
auto r = s.eval("match Some 42 with | Some n when n > 0 -> n | _ -> 0");
assert(r.is_ok());
assert(metaml::to_string(r.unwrap()) == "42");
