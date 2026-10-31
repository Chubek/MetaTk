assert(pattern!"[0-9]+".matches("2048"));
assert(!pattern!"[0-9]+".matches("20x"));
