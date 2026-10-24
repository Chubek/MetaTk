import "prelude"
val squares := List.map (fn x -> x * x) (List.filter (fn x -> x % 2 == 0) [1,2,3,4,5,6])
