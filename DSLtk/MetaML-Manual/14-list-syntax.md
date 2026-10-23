# 14. Lists and ranges

Lists are immutable singly linked sequences with one element type. `[]` is empty, `[1, 2, 3]` is a literal, and `x :: xs` constructs a new head sharing the tail. `@` concatenates two lists. Constructing a head is cheap; appending to the end requires traversing or copying the left side.

```metaml
xs := 1 :: [2, 3]
Test.equal "construction" xs [1, 2, 3]
Test.equal "concatenation" (xs @ [4]) [1, 2, 3, 4]
Test.equal "half-open library range" (List.range 2 5) [2, 3, 4]
Test.equal "inclusive syntax range" [2 .. 5] [2, 3, 4, 5]
Test.equal "descending library range" (List.range 5 2) []
```

`List.range start stop` is half-open: the stop value is excluded. The syntax `[start .. stop]` is inclusive instead. Both produce empty lists for descending bounds. An ascending request whose distance is too large raises `M0404`; the guards compute their distance without signed overflow. Use `List.range` when a half-open interval is important, and do not accidentally include an extra element by switching to range syntax.

Empty or out-of-bounds queries return `Option`: `List.head`, `tail`, `last`, and `nth`. `nth` is linear because the representation is linked. It is a poor substitute for indexed iteration in an inner loop.

Build a list by consing into a reversed accumulator and reversing once at the end. Repeatedly using `acc @ [x]` is typically quadratic. The standard `map`, filter, and fold utilities already manage this representation efficiently for ordinary workloads.

List elements may themselves be lists, records, tuples, or constructors. A list of functions is legal, but comparing it structurally is not generally legal at runtime. Use a meaningful projection or a domain-specific comparison rather than assuming every list has a useful total order.

**Related:** chapter 28 is the List API reference and recipe guide.

<!-- nav:begin -->
[Contents](README.md) · [Previous](13-tuples.md) · [Next](15-comprehensions.md)
<!-- nav:end -->
