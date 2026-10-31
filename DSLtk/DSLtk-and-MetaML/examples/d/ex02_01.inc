auto add = pipe((int x) => x + 3);
auto mul = pipe((int x) => x * 4);
assert((2 | add | mul) == 20);
