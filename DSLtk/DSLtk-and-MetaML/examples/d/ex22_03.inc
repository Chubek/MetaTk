auto s = new MetaML.Session();
auto path = s.resolve_import("__book_missing_module__");
assert(path.isNull);
