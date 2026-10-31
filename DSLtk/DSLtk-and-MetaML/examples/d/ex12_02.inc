auto units = literal_set(
  lit!"_km"((real x) => x * 1000));
assert(units.parse("2_km") == 2000);
