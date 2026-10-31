enum Mode { Read, Write }
assert(enum_name(Mode.Read) == "Read");
assert(enum_cast!Mode("Write").get == Mode.Write);
