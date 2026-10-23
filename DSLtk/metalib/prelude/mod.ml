{- metalib/prelude — opened in every session (AGENTS.md §8.3).

   Loads the standard modules and opens Core. The operators `+ - * / %`,
   `== != < > <= >=`, `&& ||`, `^` (strings), `@` (lists), `::`, `|>`,
   `>>`, `<<` and `∘` are builtin; Bool, Option and Result are builtin
   types. Removing or reordering these exports is a breaking change. -}

import "core"
import "int"
import "float"
import "string"
import "list"
import "option"
import "result"
import "io"

open Core

print :: String -> Unit
print := IO.println($1)

println :: String -> Unit
println := IO.println($1)

show_line :: 'a -> Unit
show_line := IO.println(Core.show($1))

sum :: Num 'a => ['a] -> 'a
sum := List.sum($1)

product :: Num 'a => ['a] -> 'a
product := List.product($1)

length :: ['a] -> Int
length := List.length($1)

map :: ('a -> 'b) -> ['a] -> ['b]
map := List.map($1, $2)

filter :: ('a -> Bool) -> ['a] -> ['a]
filter := List.filter($1, $2)

fold :: ('b -> 'a -> 'b) -> 'b -> ['a] -> 'b
fold := List.fold_left($1, $2, $3)

reverse :: ['a] -> ['a]
reverse := List.reverse($1)

range :: Int -> Int -> [Int]
range := List.range($1, $2)

concat :: [String] -> String
concat := String.join("", $1)
