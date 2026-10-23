-- Host modules: File, Process, FFI, CBridge
Test.section "File"
dir := File.join (File.temp_dir ()) ("metaml_test_" ^ Int.to_string (Process.pid ()))
Test.equal "mkdir" (File.mkdir (File.join dir "sub/deeper")) (Ok ())
p := File.join dir "a.txt"
Test.equal "write" (File.write p "hello\nworld\n") (Ok ())
Test.equal "append" (File.append p "!") (Ok ())
Test.equal "read" (File.read p) (Ok "hello\nworld\n!")
Test.equal "read_lines" (File.read_lines p) (Ok ["hello", "world", "!"])
Test.equal "size" (File.size p) (Ok 13)
Test.check "exists" (File.exists p && File.is_file p && File.is_dir dir && not (File.is_dir p))
Test.equal "copy" (File.copy p (File.join dir "b.txt")) (Ok ())
Test.equal "rename" (File.rename (File.join dir "b.txt") (File.join dir "c.txt")) (Ok ())
Test.equal "list_dir" (File.list_dir dir) (Ok ["a.txt", "c.txt", "sub"])
Test.equal "remove" (List.map File.remove [p, File.join dir "c.txt", File.join dir "sub/deeper", File.join dir "sub", dir]) [Ok (), Ok (), Ok (), Ok (), Ok ()]
Test.check "gone" (not (File.exists dir))
IO.println (match File.read "/no/such/file" with | Ok _ -> "?" | Err e -> e)
IO.println (match File.list_dir "/no/such/dir" with | Ok _ -> "?" | Err e -> e)
IO.println (match File.remove "/no/such/file" with | Ok _ -> "?" | Err e -> e)
Test.equal "basename" (List.map File.basename ["a/b.txt", "a/b/", "/", "x", ""]) ["b.txt", "b", "/", "x", ""]
Test.equal "dirname" (List.map File.dirname ["a/b.txt", "b.txt", "/x", "a/b/", "/a/b//c"]) ["a", ".", "/", "a", "/a/b"]
Test.equal "extension" (List.map File.extension ["b.tar.gz", ".bashrc", "noext", "d.ir/x"]) [".gz", "", "", ""]
Test.equal "join" (File.join "a" "b", File.join "a/" "b", File.join "a" "/b", File.join "" "b") ("a/b", "a/b", "/b", "b")

Test.section "Process"
Test.equal "run" (Process.run "printf" ["%s-%s", "a", "b"]) (Ok (0, "a-b", ""))
Test.equal "run stderr" (Process.run "sh" ["-c", "echo out; echo err >&2; exit 3"]) (Ok (3, "out\n", "err\n"))
IO.println (match Process.run "no_such_program_xyz" [] with | Ok _ -> "?" | Err e -> e)
Test.equal "shell" (Process.shell "echo $((6 * 7))") (Ok (0, "42\n"))
Process.set_env "METAML_TEST_VAR" "v1"
Test.equal "env" (Process.env "METAML_TEST_VAR", Process.env "METAML_NO_SUCH_VAR") (Some "v1", None)
Test.equal "args" (Process.args ()) ["one", "two"]
Test.check "pid" (Process.pid () > 0)
t0 := Process.clock_ms ()
Test.check "clock" (Process.clock_ms () >= t0 && Process.time () > 1.6e9)
Test.check "cwd" (String.length (Process.cwd ()) > 0)
IO.println (match Process.chdir "/no/such/dir" with | Ok _ -> "?" | Err e -> e)

Test.section "FFI"
Test.check "available" (FFI.available ())
Test.equal "cos" (FFI.call "libm.so.6" "cos" "d(d)" [FFloat 0.0]) (Ok (FFloat 1.0))
Test.equal "pow" (FFI.call "libm.so.6" "pow" "d(dd)" [FFloat 2.0, FFloat 10.0]) (Ok (FFloat 1024.0))
Test.equal "strlen" (FFI.call "" "strlen" "l(s)" [FStr "hello"]) (Ok (FInt 5))
Test.equal "abs" (FFI.call "" "abs" "i(i)" [FInt (~42)]) (Ok (FInt 42))
Test.equal "getpid" (FFI.call "" "getpid" "i()" []) (Ok (FInt (Process.pid ())))
Test.equal "getenv" (FFI.call "" "getenv" "s(s)" [FStr "METAML_TEST_VAR"]) (Ok (FStr "v1"))
Test.check "symbol_exists" (FFI.symbol_exists "" "strlen" && not (FFI.symbol_exists "" "no_such_symbol_xyz"))
IO.println (match FFI.call "" "strlen" "l(s" [] with | Ok _ -> "?" | Err e -> e)
IO.println (match FFI.call "" "strlen" "l(s)" [] with | Ok _ -> "?" | Err e -> e)
IO.println (match FFI.call "" "strlen" "l(s)" [FInt 1] with | Ok _ -> "?" | Err e -> e)
IO.println (match FFI.call "" "no_such_symbol_xyz" "v()" [] with | Ok _ -> "?" | Err e -> e)
IO.println (match FFI.call "libno_such_lib.so" "f" "v()" [] with | Ok _ -> "?" | Err e -> String.sub 0 50 e)

Test.section "CBridge"
Test.equal "api" (CBridge.api_version ()) 1
Test.check "functions" (List.mem "metaml_eval" (CBridge.functions ()))
