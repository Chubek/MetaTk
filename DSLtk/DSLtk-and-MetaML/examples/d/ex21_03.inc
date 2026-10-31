auto s = new MetaML.Session();
int state;
s.on_close(() { state = 1; }, () { assert(state == 1); state = 2; });
s.close();
assert(state == 2);
