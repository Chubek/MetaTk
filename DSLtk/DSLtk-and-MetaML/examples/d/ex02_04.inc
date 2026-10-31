auto table = match(
  when!1((int key) => 10),
  otherwise((int key) => -1));
assert(table(1) == 10);
assert(table(2) == -1);
