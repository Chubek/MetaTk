metaml::Session s;
auto path = s.resolve_import("__book_missing_module__");
assert(!path.has_value());
