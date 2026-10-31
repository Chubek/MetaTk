auto metres = (string text) {
  auto ls = literal_set(lit!"_m"(
    (real n) => n));
  return ls.parse(text);
};
assert(metres("12_m") == 12);
