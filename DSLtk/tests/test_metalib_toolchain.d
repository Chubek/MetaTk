import metalib.d.Support;
import metalib.xvm.XVM;
import metalib.arch.Arch;
import metalib.chunk.Chunk;
import metalib.objfile.Objfile;
import metalib.xvm.Backend : Backend;
import metalib.ast.Absyn : readFile;
import std.path : buildPath, dirName;
import DSLtk : Span;
import std.stdio : writeln, stderr;

Value eval(Session s, string source)
{
    auto r = s.eval(source);
    if (r.is_err()) throw new Exception(source ~ "\n" ~ s.render(r.unwrap_err()[0]));
    return r.unwrap();
}
int main(string[] args)
{
    try {
        assert(args.length == 2 || args.length == 3);
        auto s = new Session;
        assert(s.interp().engine == ExecutionEngine.XVM);
        scope(exit) s.close();
        installToolchain(s);
        auto before = s.find_primitive("Chunk.of_source");
        installToolchain(s);
        assert(before is s.find_primitive("Chunk.of_source"));
        assert(s.eval("Chunk.of_source(\"42\")").is_err());
        s.allow_all_primitives();
        assert(s.load_stdlib(args[1]).is_ok());
        eval(s, "import \"ast\"\nimport \"system\"\nimport \"objfile\"\nimport \"xvm\"");
        void check(string expression) { assert(eval(s, expression).i != 0, expression); }
        check("match Absyn.of_source \"val x := 42\" with | Ok tree -> Absyn.tag tree == \"Program\" | Err _ -> false");
        check("match Absyn.current () with | Ok tree -> Absyn.Visitor.fold (fn n -> fn _ -> n + 1) 0 tree > 1 | Err _ -> false");
        eval(s, "module Host := System(Posix)\nmodule Object := Objfile(ELF)\nmodule CPU := Arch(Amd64)\nmodule M := XVM(Amd64)");
        check("Host.name () == \"posix\"");
        check("CPU.word_bits () == 64");
        check("match Object.of_code \"amd64\" (CPU.return_int 42) with | Ok bytes -> Object.matches bytes | Err _ -> false");
        check("match Chunk.of_source \"40 + 2\" with | Ok chunk -> match M.run chunk with | Ok value -> M.show value == \"42\" | Err _ -> false | Err _ -> false");
        check("match Chunk.of_source \"40 + 2\" with | Ok chunk -> match Chunk.decode (Chunk.encode chunk) with | Ok restored -> Chunk.source restored == \"40 + 2\" | Err _ -> false | Err _ -> false");
        check("match Chunk.decode \"XVM\" with | Err _ -> true | Ok _ -> false");
        check("match Chunk.current () with | Ok chunk -> Chunk.filename chunk == \"<eval>\" | Err _ -> false");
        string output;
        s.set_output((string text) { output ~= text; });
        auto contracts = buildPath(dirName(args[1]), "tests/toolchain/contracts.mml");
        auto contractResult = s.run_source(readFile(contracts), contracts, RunMode.Eval);
        assert(contractResult.ok(), contractResult.ok() ? "" : s.render(contractResult.errors[0]));
        assert(output == "rootleftrightleftrightrootroot");
        assert(s.interp().eval_depth == 0 && s.interp().call_depth == 0);
        assert(s.current_program() is null);
        foreach (format; ["elf", "pe", "coff", "macho"]) {
            foreach (arch; ["amd64", "aarch64", "riscv64"]) {
                if (format == "macho" && arch == "riscv64") continue;
                auto bytes = ofCode(format, arch, "\xc3");
                auto info = inspect(format, bytes);
                assert(info.arch == arch && info.bits == 64 && info.sections > 0);
                bool failed;
                try inspect(format, bytes[0 .. $ / 2]);
                catch (Exception e) failed = true;
                assert(failed);
            }
        }
        bool canJit;
        {
            auto probe = new Session;
            scope(exit) probe.close();
            probe.interp().engine = ExecutionEngine.NativeXVM;
            auto result = probe.eval("42");
            canJit = result.is_ok();
            if (canJit) assert(result.unwrap().i == 42);
            else assert(result.unwrap_err()[0].code == "M0409");
        }
        string[] programs = [
            "40 + 2",
            "val make x := fn y -> x + y\nval add := make 40\nadd 2",
            "fn x -> x + 1",
            "val fact n := if n <= 1 then 1 else n * fact (n - 1)\nfact 6",
            "type Tree := | Tip Int | Fork Tree Tree\nval sum t := match t with | Tip n -> n | Fork a b -> sum a + sum b\nsum (Fork (Tip 20) (Tip 22))",
            "module type S := sig val x : Int end\nmodule F (N : S) := struct val y := N.x + 1 end\nmodule A := struct val x := 41 end\nmodule B := F(A)\nB.y",
            "val get r := r.x\nget {x := 42, y := true}",
            "[x * 2 for x <- [1, 2, 3]]", "import \"list\"\nList.reverse [1, 2, 3]",
            "val _ := IO.println(\"effect\")\n42",
            "1 / 0", "true + 1"
        ];
        foreach (source; programs) {
            foreach (jit; [false, true]) {
                if (jit && !canJit) continue;
                auto reference = new Session, actual = new Session;
                reference.interp().engine = ExecutionEngine.Tree;
                string referenceOutput, actualOutput;
                reference.set_output((string text) { referenceOutput ~= text; });
                actual.set_output((string text) { actualOutput ~= text; });
                scope(exit) { reference.close(); actual.close(); }
                reference.allow_all_primitives(); actual.allow_all_primitives();
                assert(reference.load_stdlib(args[1]).is_ok());
                assert(actual.load_stdlib(args[1]).is_ok());
                auto expected = reference.run_source(source, "test.mml", RunMode.Eval);
                assert(expected.ok() == (source != "1 / 0" && source != "true + 1"), "invalid test program: " ~ source);
                auto backend = new Backend;
                scope(exit) backend.close();
                actual.interp().xvm_instructions = actual.interp().xvm_native_blocks = 0;
                auto result = backend.eval(actual, source, "test.mml", jit);
                assert(expected.ok() == result.ok());
                assert(referenceOutput == actualOutput);
                assert(reference.interp().xvm_instructions == 0);
                if (source != "true + 1") {
                    assert(actual.interp().xvm_instructions > 0);
                    assert((actual.interp().xvm_native_blocks > 0) == jit);
                }
                if (expected.ok()) assert(show_value(expected.value.get()) == show_value(result.value.get()));
                else assert(expected.errors[0].code == result.errors[0].code);
                assert(actual.current_program() is null);
            }
        }
        auto chunk = compile("val x := 42\nx");
        auto vm = new VM;
        foreach (i; 0 .. 3) assert(vm.run(s, chunk).value.get().i == 42);
        assert(encode(decode(encode(chunk))) == encode(chunk));
        if (args.length == 3) {
            import std.file : write;
            auto path = buildPath(args[2], "toolchain-d.xvm");
            write(path, encode(chunk));
            auto restored = fromFile(path);
            assert(restored.source == chunk.source && restored.file == chunk.file);
        }
        auto encoded = encode(chunk).dup;
        encoded[24] = 99;
        bool malformed;
        try decode(cast(string)encoded);
        catch (Exception e) malformed = true;
        assert(malformed);
        encoded[24] = 1; encoded[8 .. 16] = '\xff'; malformed = false;
        try decode(cast(string)encoded);
        catch (Exception e) malformed = true;
        assert(malformed);
        auto restricted = new Session;
        scope(exit) restricted.close();
        installToolchain(restricted);
        auto denied = vm.run(restricted, compile("Posix.cwd(())"));
        assert(!denied.ok() && denied.errors[0].code == "M0403");
        auto other = host() == Target.Amd64 ? Target.Aarch64 : Target.Amd64;
        bool failed;
        try (new VM(other)).run(s, chunk, true);
        catch (Exception e) failed = true;
        assert(failed);
        long composedReference;
        foreach (engine; [ExecutionEngine.Tree, ExecutionEngine.PortableXVM, ExecutionEngine.XVM]) {
            auto reuse = new Session;
            scope(exit) reuse.close();
            reuse.interp().engine = engine;
            eval(reuse, "val f x := x + 1\nval g := 41");
            auto parsed = parse("f g", "reuse.mml");
            assert(parsed.ok());
            auto first = reuse.run_program(parsed.program, RunMode.Eval);
            assert(first.ok() && first.value.get().i == 42);
            eval(reuse, "val g x := x * 2");
            auto second = reuse.run_program(parsed.program, RunMode.Eval);
            assert(second.ok() && !second.value.isNull);
            auto value = reuse.interp().apply(second.value.get(), [Value.integer(20)], Span.init).i;
            if (engine == ExecutionEngine.Tree) composedReference = value;
            else assert(value == composedReference);
        }
        {
            auto bounded = new Session;
            scope(exit) bounded.close();
            bounded.interp().engine = ExecutionEngine.PortableXVM;
            bounded.interp().xvm_code_limit = 1;
            auto error = bounded.eval("42");
            assert(error.is_err() && error.unwrap_err()[0].code == "M0406");
            bounded.interp().xvm_code_limit = 1000;
            bounded.interp().xvm_stack_limit = 1;
            error = bounded.eval("1 + 2");
            assert(error.is_err() && error.unwrap_err()[0].code == "M0406");
            bounded.interp().xvm_stack_limit = 1000;
            bounded.interp().cancelled = () => true;
            error = bounded.eval("42");
            assert(error.is_err() && error.unwrap_err()[0].code == "M0410");
            bounded.interp().cancelled = null;
            if (canJit) {
                bounded.interp().engine = ExecutionEngine.NativeXVM;
                bounded.interp().xvm_native_limit = 1;
                foreach (attempt; 0 .. 2) {
                    error = bounded.eval("1 + 2");
                    assert(error.is_err() && error.unwrap_err()[0].code == "M0406");
                }
                bounded.interp().xvm_native_limit = 1024 * 1024;
            }
            assert(eval(bounded, "40 + 2").i == 42);
        }
        {
            auto direct = new Session;
            scope(exit) direct.close();
            direct.interp().engine = ExecutionEngine.PortableXVM;
            auto function_ = eval(direct, "fn x -> x + 1");
            direct.interp().xvm_code_limit = 1;
            bool limited;
            try direct.interp().apply(function_, [Value.integer(41)], Span.init);
            catch (MetaError e) limited = e.diag.code == "M0406";
            assert(limited && !direct.interp().call_depth && !direct.interp().eval_depth);
            direct.interp().xvm_code_limit = 1000;
            assert(direct.interp().apply(function_, [Value.integer(41)], Span.init).i == 42);
        }
        writeln("metalib toolchain tests passed");
        return 0;
    } catch (Exception e) { stderr.writeln(e.msg); return 1; }
}
