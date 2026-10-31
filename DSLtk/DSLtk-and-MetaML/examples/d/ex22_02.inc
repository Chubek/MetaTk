auto s = new MetaML.Session();
s.add_search_path("modules");
auto paths = s.search_paths();
assert(paths.length > 0);
assert(paths[0] == "modules");
