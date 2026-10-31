auto s = new MetaML.Session();
auto r = s.load_stdlib("__book_missing_metalib__");
assert(r.is_err());
assert(!s.stdlib_loaded());
