import "prelude"
import "fmt"
Test.equal "escaped substitution" (Fmt.format "{{{}}}" ["x"]) (Ok "{x}")
Test.equal "missing argument" (Fmt.format "{}" []) (Err "Fmt: missing argument")
Test.equal "chunks" (List.chunks_checked 2 [1,2,3,4,5]) (Some [[1,2],[3,4],[5]])
Test.equal "invalid chunk size" (List.chunks_checked 0 [1,2]) None
Test.equal "assoc first match" (List.assoc "x" [("x",1),("x",2)]) (Some 1)
