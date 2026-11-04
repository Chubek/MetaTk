{- metalib/list — the List module and list utilities.

   Lists are immutable singly linked lists. Every function here is
   tail-recursive (or host-implemented), so they work on long lists.
   Comprehensions `[e for x <- xs if p]` are built into the language.
   Host primitives used: List.length, List.reverse, List.nth, List.range,
   List.sum, List.product, List.sort, List.sort_by, List.concat,
   List.append. -}

module type LIST := sig
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
end

module List :> LIST := struct
  val empty := []
  val singleton x := [x]
  val cons := $1 :: $2
  val length := List.length($1)
  val is_empty xs := match xs with | [] -> true | _ -> false
  val head xs := match xs with | [] -> None | x :: _ -> Some x
  val tail xs := match xs with | [] -> None | _ :: rest -> Some rest
  val last xs := match List.reverse(xs) with | [] -> None | x :: _ -> Some x
  val nth := List.nth($1, $2)
  val reverse := List.reverse($1)
  val append := List.append($1, $2)
  val concat := List.concat($1)
  val fold_left f acc xs := match xs with
    | [] -> acc
    | x :: rest -> fold_left f (f acc x) rest
  val fold_right f xs init := fold_left (fn acc x -> f x acc) init (List.reverse(xs))
  val map f xs := List.reverse(fold_left (fn acc x -> f x :: acc) [] xs)
  val mapi f xs :=
    let go acc p := match p with | (i, x) -> f i x :: acc in
    List.reverse(fold_left go [] (enumerate xs))
  val filter p xs := List.reverse(fold_left (fn acc x -> if p x then x :: acc else acc) [] xs)
  val filter_map f xs :=
    let step acc x := match f x with | Some y -> y :: acc | None -> acc in
    List.reverse(fold_left step [] xs)
  val flat_map f xs := List.concat(map f xs)
  val for_each f xs := fold_left (fn _ x -> f x) () xs
  val any p xs := match xs with
    | [] -> false
    | x :: rest -> if p x then true else any p rest
  val all p xs := match xs with
    | [] -> true
    | x :: rest -> if p x then all p rest else false
  val exists := any $1 $2
  val find p xs := match xs with
    | [] -> None
    | x :: rest -> if p x then Some x else find p rest
  val mem x xs := any (fn y -> y == x) xs
  val count p xs := fold_left (fn n x -> if p x then n + 1 else n) 0 xs
  val partition p xs :=
    let step acc x := match acc with
      | (yes, no) -> if p x then (x :: yes, no) else (yes, x :: no) in
    match fold_left step ([], []) xs with
    | (yes, no) -> (List.reverse(yes), List.reverse(no))
  val take n xs :=
    let rec go k ys acc := match ys with
      | [] -> List.reverse(acc)
      | y :: rest -> if k <= 0 then List.reverse(acc) else go (k - 1) rest (y :: acc) in
    go n xs []
  val drop n xs := match xs with
    | [] -> []
    | _ :: rest -> if n <= 0 then xs else drop (n - 1) rest
  val zip xs ys :=
    let rec go a b acc := match (a, b) with
      | (x :: ra, y :: rb) -> go ra rb ((x, y) :: acc)
      | _ -> List.reverse(acc) in
    go xs ys []
  val unzip ps :=
    let step acc p := match (acc, p) with
      | ((xs, ys), (x, y)) -> (x :: xs, y :: ys) in
    match fold_left step ([], []) ps with
    | (xs, ys) -> (List.reverse(xs), List.reverse(ys))
  val enumerate xs := zip (List.range(0, List.length(xs))) xs
  val range := List.range($1, $2)
  val replicate n x := map (fn _ -> x) (List.range(0, n))
  val sum := List.sum($1)
  val product := List.product($1)
  val sort := List.sort($1)
  val sort_by := List.sort_by($1, $2)
  val uniq xs := List.reverse(fold_left (fn acc x -> if mem x acc then acc else x :: acc) [] xs)
  val intersperse sep xs := match xs with
    | [] -> []
    | x :: rest -> x :: flat_map (fn y -> [sep, y]) rest
  val maximum xs := match xs with
    | [] -> None
    | x :: rest -> Some (fold_left (fn m y -> if y > m then y else m) x rest)
  val minimum xs := match xs with
    | [] -> None
    | x :: rest -> Some (fold_left (fn m y -> if y < m then y else m) x rest)
end
