metaml::Session s;
s.add_search_path("modules");
auto paths = s.search_paths();
assert(!paths.empty());
assert(paths.front() == "modules");
