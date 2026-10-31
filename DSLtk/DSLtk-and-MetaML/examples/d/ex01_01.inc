struct Domain { mixin DSL!(Pipeline); }
Domain domain;
auto stage = pipe((int x) => x + 1);
assert((41 | stage) == 42);
