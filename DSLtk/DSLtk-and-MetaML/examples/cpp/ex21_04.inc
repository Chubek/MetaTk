metaml::Session s;
int stops = 0;
s.on_close([&] { ++stops; });
s.close();
s.close();
assert(stops == 1);
