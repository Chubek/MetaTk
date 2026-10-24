import "parsec"
val parser := Parser.map (fn _ -> 42) (Parser.string "answer")
val parsed := Parser.run parser "answer"
