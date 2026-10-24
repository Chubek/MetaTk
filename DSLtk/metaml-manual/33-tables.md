# 33. Table: deterministic mutable maps

Table is an opaque string-keyed mutable map. Keys are strings, while a particular table should contain one consistent value type. `create ()` starts empty, and `of_list` builds a map from key/value pairs, with later duplicate keys replacing earlier values.

```metaml
t := Table.of_list [("b", 2), ("a", 1)]
snapshot := Table.clone t
Test.check "new key" (Table.set_if_absent "c" 3 t)
Test.check "existing key" (not (Table.set_if_absent "a" 9 t))
Test.equal "ordered values" (Table.values t) [1, 2, 3]
Table.remove "a" t
Test.equal "independent map" (Table.to_list snapshot) [("a", 1), ("b", 2)]
```

`set` inserts or replaces; `get` returns Option; `has` checks presence; `remove` is harmless for a missing key. `size`, `keys`, `values`, and `to_list` inspect the map. Enumeration is deterministic in sorted-key order in both host dialects, even though their native map representations differ.

`clone` copies the map structure under its lock. The values are shallow copies: immutable lists and records remain shared safely, while nested foreign handles still refer to the same underlying resources. Clone is not a deep serialization mechanism.

`set_if_absent` performs the lookup and insertion under one lock and returns true only for a newly inserted key. This is preferable to a separate `has`/`set` pair if concurrent host code can touch the same handle. No user callback runs while holding the map lock.

The language currently has no general value restriction for mutable host handles. Keep table usage homogeneous and consider explicit annotations or narrow wrapper functions. Do not exploit a generalized `Table 'v` binding to mix unrelated types; static polymorphism does not imply independent storage per type instantiation.

**Sources:** [`cpp/Table.h`](../metalib/cpp/Table.h), [`d/Table.d`](../metalib/d/Table.d).

<!-- nav:begin -->
[Contents](README.md) · [Previous](32-buffers.md) · [Next](34-hashes-and-uris.md)
<!-- nav:end -->
