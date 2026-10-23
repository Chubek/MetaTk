{- metalib/io — console input and output (embedding dependent).

   Output goes to the session's output sink (stdout in the REPL; the
   embedder may redirect it). Host primitives used: IO.print, IO.println,
   IO.eprint, IO.read_line, Core.show. -}

module type IO := sig
  val print : String -> Unit
  val println : String -> Unit
  val eprint : String -> Unit
  val print_value : 'a -> Unit
  val read_line : Unit -> Option String
end

module IO :> IO := struct
  val print := IO.print($1)
  val println := IO.println($1)
  val eprint := IO.eprint($1)
  val print_value x := IO.println(Core.show(x))
  val read_line := IO.read_line($1)
end
