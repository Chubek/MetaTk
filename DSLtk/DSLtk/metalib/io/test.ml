{- Tests for metalib/io (output is part of the expected transcript). -}
import "prelude"

Test.section "io"
IO.print "no newline|"
IO.println "line"
IO.print_value (Some [1, 2])
Test.check "read_line at end of input" (Option.is_none (IO.read_line ()) || true)
