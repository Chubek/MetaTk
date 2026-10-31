auto twice = pipe((int x) => x * 2);
auto answer = 21 | twice;
assert(answer == 42);
