auto s = new MetaML.Session();
int stops;
s.on_close(() { ++stops; });
s.close();
s.close();
assert(stops == 1);
