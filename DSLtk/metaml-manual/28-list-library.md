# 28. List: traversals and collection recipes

List's public API emphasizes immutable transformations and data-last arguments. `map`, `filter`, `filter_map`, and `flat_map` cover common transformations. `fold_left f init xs` visits elements left to right; `fold_right f xs init` combines from the right using reversed traversal rather than naive host recursion.

```metaml
Test.equal "scan includes seed" (List.scan (+) 0 [1, 2, 3]) [0, 1, 3, 6]
Test.equal "split" (List.split_at 2 [1, 2, 3]) ([1, 2], [3])
Test.equal "span stops once" (List.span (< 3) [1, 2, 3, 1]) ([1, 2], [3, 1])
Test.equal "zip shortest" (List.zip_with (+) [1, 2] [10]) [11]
Test.equal "index" (List.find_index (> 2) [1, 3, 4]) (Some 1)
```

`split_at` clamps a non-positive count to an empty prefix. `span` returns the longest satisfying prefix and the untouched remainder; `take_while` and `drop_while` select its components. `find_index` returns the first zero-based match. `scan` includes the initial accumulator, including for empty input. These helpers are available in portable sessions without installing host extensions.

`any`, `all`, and `find` short-circuit. `partition` evaluates the predicate over the whole input and preserves order within both outputs. `zip` and `zip_with` stop at the shorter input. `uniq` preserves first occurrences but can be quadratic because it uses structural membership checks.

Sorting is stable. `sort_by` expects a negative/zero/positive comparator result; comparator callbacks may fail with ordinary diagnostics. Avoid inconsistent comparators or NaN-dependent ordering.

The host List module adds chunks, windows, runs, transpose, seeded shuffle, rotation, and comparator-based extrema. A window result can be substantially larger than its input. Prefer portable functions when they already express the operation clearly, and install host extensions only where needed.

**Source:** [`list/mod.ml`](../metalib/list/mod.ml).

<!-- api:begin -->
```text
  val empty : ['a]
  val singleton : 'a -> ['a]
  val cons : 'a -> ['a] -> ['a]
  val length : ['a] -> Int
  val is_empty : ['a] -> Bool
  val head : ['a] -> Option 'a
  val tail : ['a] -> Option ['a]
  val last : ['a] -> Option 'a
  val nth : Int -> ['a] -> Option 'a
  val reverse : ['a] -> ['a]
  val append : ['a] -> ['a] -> ['a]
  val concat : [['a]] -> ['a]
  val map : ('a -> 'b) -> ['a] -> ['b]
  val mapi : (Int -> 'a -> 'b) -> ['a] -> ['b]
  val filter : ('a -> Bool) -> ['a] -> ['a]
  val filter_map : ('a -> Option 'b) -> ['a] -> ['b]
  val flat_map : ('a -> ['b]) -> ['a] -> ['b]
  val fold_left : ('b -> 'a -> 'b) -> 'b -> ['a] -> 'b
  val fold_right : ('a -> 'b -> 'b) -> ['a] -> 'b -> 'b
  val for_each : ('a -> Unit) -> ['a] -> Unit
  val any : ('a -> Bool) -> ['a] -> Bool
  val all : ('a -> Bool) -> ['a] -> Bool
  val exists : ('a -> Bool) -> ['a] -> Bool
  val find : ('a -> Bool) -> ['a] -> Option 'a
  val mem : 'a -> ['a] -> Bool
  val count : ('a -> Bool) -> ['a] -> Int
  val partition : ('a -> Bool) -> ['a] -> (['a], ['a])
  val take : Int -> ['a] -> ['a]
  val drop : Int -> ['a] -> ['a]
  val zip : ['a] -> ['b] -> [('a, 'b)]
  val unzip : [('a, 'b)] -> (['a], ['b])
  val enumerate : ['a] -> [(Int, 'a)]
  val range : Int -> Int -> [Int]
  val replicate : Int -> 'a -> ['a]
  val sum : Num 'a => ['a] -> 'a
  val product : Num 'a => ['a] -> 'a
  val sort : ['a] -> ['a]
  val sort_by : ('a -> 'a -> Int) -> ['a] -> ['a]
  val uniq : ['a] -> ['a]
  val intersperse : 'a -> ['a] -> ['a]
  val maximum : ['a] -> Option 'a
  val minimum : ['a] -> Option 'a
  val split_at : Int -> ['a] -> (['a], ['a])
  val take_while : ('a -> Bool) -> ['a] -> ['a]
  val drop_while : ('a -> Bool) -> ['a] -> ['a]
  val span : ('a -> Bool) -> ['a] -> (['a], ['a])
  val zip_with : ('a -> 'b -> 'c) -> ['a] -> ['b] -> ['c]
  val scan : ('b -> 'a -> 'b) -> 'b -> ['a] -> ['b]
  val find_index : ('a -> Bool) -> ['a] -> Option Int
```
<!-- api:end -->

<!-- nav:begin -->
[Contents](README.md) · [Previous](27-string-library.md) · [Next](29-option-library.md)
<!-- nav:end -->
