metaml::Session s;
std::string captured;
s.set_output([&](std::string_view x) { captured += x; });
s.write("hello");
assert(captured == "hello");
