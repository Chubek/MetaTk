string source = "one\ntwo";
auto map = SourceMap(source);
auto pos = map.line_column(4);
assert(pos[0] == 2 && pos[1] == 1);
assert(map.line_text(2) == "two");
