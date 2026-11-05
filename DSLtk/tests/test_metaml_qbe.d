import MetaML;
import metaml_backends.QBE : QBEBackend = QBE;
import std.stdio : writeln, stderr;
import DSLtk : Span;

void error(RunReport report, string code) {
    assert(!report.ok() && report.errors[0].code == code, "expected " ~ code);
}
int main() {
    try {
        assert(execution_engine_from_name("qbe").get == ExecutionEngine.QBE);
        auto backend = new QBEBackend;
        auto session = new Session;
        scope(exit) session.close();
        auto saved = session.interp().engine;
        auto checked = backend.eval(session, "true + 1", "check.mml");
        assert(!checked.ok() && !session.interp().qbe_native_blocks);
        assert(session.interp().engine == saved);
        version(METAML_NO_JIT) enum enabled = false;
        else version(METAML_ENABLE_QBE) enum enabled = true;
        else enum enabled = false;
        static if (enabled) {
            string[] programs = [
                "40 + 2", "1.25 + 2.5", "\"hello\" ^ \" world\"",
                "val make x := fn y -> x + y\nval add := make 40\nadd 2", "fn x -> x + 1",
                "val fact n := if n <= 1 then 1 else n * fact (n - 1)\nfact 6",
                "val loop n acc := if n == 0 then acc else loop (n - 1) (acc + 1)\nloop 10000 0",
                "type Tree := | Tip Int | Fork Tree Tree\nval sum t := match t with | Tip n -> n | Fork a b -> sum a + sum b\nsum (Fork (Tip 20) (Tip 22))",
                "module type S := sig val x : Int end\nmodule F (M : S) := struct val y := M.x + 1 end\nmodule A := struct val x := 41 end\nmodule B := F(A)\nB.y",
                "let r := {x := 1, y := 2} in ({r with x := 42}).x",
                "[x * 2 for x <- [1, 2, 3]]", "[1 .. 5]", "(1, true, \"tuple\")",
                "false && (1 / 0 == 0)", "true || (1 / 0 == 0)",
                "val _ := IO.println(\"effect\")\n42", "1 / 0", "true + 1"
            ];
            foreach (source; programs) {
                auto reference = new Session, actual = new Session;
                scope(exit) { reference.close(); actual.close(); }
                reference.interp().engine = ExecutionEngine.Tree;
                reference.allow_all_primitives(); actual.allow_all_primitives();
                string referenceOutput, actualOutput;
                reference.set_output((string text) { referenceOutput ~= text; });
                actual.set_output((string text) { actualOutput ~= text; });
                auto expected = reference.run_source(source, "qbe-test.mml", RunMode.Eval);
                assert(expected.ok() == (source != "1 / 0" && source != "true + 1"), "invalid fixture: " ~ source);
                auto result = backend.eval(actual, source, "qbe-test.mml");
                assert(result.ok() == expected.ok(), source ~ (result.ok() ? "" : "\n" ~ actual.render(result.errors[0])));
                assert(referenceOutput == actualOutput);
                if (expected.ok()) assert(show_value(expected.value.get()) == show_value(result.value.get()));
                else assert(reference.render(expected.errors[0]) == actual.render(result.errors[0]));
                if (source != "true + 1") assert(actual.interp().qbe_native_blocks > 0);
                assert(actual.interp().engine == saved && actual.current_program() is null);
                assert(!actual.interp().eval_depth && !actual.interp().call_depth);
            }
            session.interp().engine = ExecutionEngine.QBE;
            assert(session.eval("val f x := x + 1\nval g := 41").is_ok());
            auto parsed = parse("f g", "reuse.mml");
            foreach (engine; [ExecutionEngine.QBE, ExecutionEngine.PortableXVM, ExecutionEngine.XVM, ExecutionEngine.QBE]) {
                session.interp().engine = engine;
                auto result = session.run_program(parsed.program, RunMode.Eval);
                assert(result.ok() && result.value.get().i == 42);
            }
            assert(session.eval("val g x := x * 2").is_ok());
            auto composed = session.run_program(parsed.program, RunMode.Eval);
            assert(composed.ok() && session.interp().apply(composed.value.get(), [Value.integer(20)], Span.init).i == 42);
            auto closure = session.eval("fn x -> x + 1").unwrap();
            foreach (engine; [ExecutionEngine.QBE, ExecutionEngine.PortableXVM, ExecutionEngine.XVM, ExecutionEngine.QBE]) {
                session.interp().engine = engine;
                assert(session.interp().apply(closure, [Value.integer(41)], Span.init).i == 42);
            }
            session.interp().xvm_native_limit = 1;
            bool limited;
            try session.interp().apply(closure, [Value.integer(41)], Span.init);
            catch (MetaError e) limited = e.diag.code == "M0406";
            assert(limited && !session.interp().eval_depth && !session.interp().call_depth);
            session.interp().xvm_native_limit = 1024 * 1024;
            assert(session.interp().apply(closure, [Value.integer(41)], Span.init).i == 42);
            auto restricted = new Session;
            scope(exit) restricted.close();
            restricted.install_primitive("Host.answer", "Unit -> Int", 1,
                (PrimContext c, Value[] a) => Value.integer(42));
            error(backend.eval(restricted, "Host.answer(())"), "M0403");
            restricted.set_primitive_allowlist(["Host.answer"]);
            assert(backend.eval(restricted, "Host.answer(())").value.get().i == 42);
            restricted.register_primitive("Host.nested", "Unit -> Int", 1,
                (PrimContext c, Value[] a) => c.session.eval("20 + 22").unwrap());
            assert(backend.eval(restricted, "Host.nested(()) + 0").value.get().i == 42);
            auto bounded = new Session;
            scope(exit) bounded.close();
            bounded.interp().xvm_code_limit = 1;
            error(backend.eval(bounded, "42"), "M0406");
            bounded.interp().xvm_code_limit = 1000;
            bounded.interp().xvm_native_limit = 1;
            foreach (i; 0 .. 2) error(backend.eval(bounded, "40 + 2"), "M0406");
            bounded.interp().xvm_native_limit = 1024 * 1024;
            bounded.interp().xvm_stack_limit = 1;
            error(backend.eval(bounded, "40 + 2"), "M0406");
            bounded.interp().xvm_stack_limit = 1000;
            bounded.interp().cancelled = () => true;
            error(backend.eval(bounded, "40 + 2"), "M0410");
            bounded.interp().cancelled = null;
            assert(backend.eval(bounded, "40 + 2").value.get().i == 42);
            bounded.interp().xvm_native_limit = 0;
            error(backend.eval(bounded, "42"), "M0409");
            // close() explicitly unloads modules, including with GC disabled.
            bounded.close(); bounded.close();
        } else {
            error(backend.eval(session, "42"), "M0409");
            assert(session.interp().engine == saved);
        }
        writeln("MetaML QBE D tests passed");
        return 0;
    } catch (Throwable e) { stderr.writeln(e); return 1; }
}
