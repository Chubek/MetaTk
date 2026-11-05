#include "../metaml-backends/QBE.hpp"
#include <iostream>
#include <stdexcept>

static void require(bool condition, const std::string &message)
{
  if (!condition) throw std::runtime_error(message);
}
static void error(const metaml::RunReport &report, const std::string &code)
{
  require(!report.ok() && report.errors.front().code == code, "expected " + code);
}
int main()
{
  try {
    using namespace metaml;
    require(execution_engine_from_name("qbe") == ExecutionEngine::QBE, "engine selection");
    backends::QBE backend;
    Session session;
    auto saved = session.interp().engine;
    auto checked = backend.eval(session, "true + 1", "check.mml");
    require(!checked.ok() && !session.interp().qbe_native_blocks, "check before native execution");
    require(session.interp().engine == saved, "restore engine after check");
#if defined(METAML_ENABLE_QBE) && !defined(METAML_NO_JIT)
    const std::vector<std::string> programs{
      "40 + 2", "1.25 + 2.5", "\"hello\" ^ \" world\"",
      "val make x := fn y -> x + y\nval add := make 40\nadd 2",
      "fn x -> x + 1",
      "val fact n := if n <= 1 then 1 else n * fact (n - 1)\nfact 6",
      "val loop n acc := if n == 0 then acc else loop (n - 1) (acc + 1)\nloop 10000 0",
      "type Tree := | Tip Int | Fork Tree Tree\nval sum t := match t with | Tip n -> n | Fork a b -> sum a + sum b\nsum (Fork (Tip 20) (Tip 22))",
      "module type S := sig val x : Int end\nmodule F (M : S) := struct val y := M.x + 1 end\nmodule A := struct val x := 41 end\nmodule B := F(A)\nB.y",
      "let r := {x := 1, y := 2} in ({r with x := 42}).x",
      "[x * 2 for x <- [1, 2, 3]]", "[1 .. 5]", "(1, true, \"tuple\")",
      "false && (1 / 0 == 0)", "true || (1 / 0 == 0)",
      "val _ := IO.println(\"effect\")\n42", "1 / 0", "true + 1"
    };
    for (const auto &source : programs) {
      Session reference, actual;
      reference.interp().engine = ExecutionEngine::Tree;
      reference.allow_all_primitives(); actual.allow_all_primitives();
      std::string reference_output, actual_output;
      reference.set_output([&](std::string_view text) { reference_output += text; });
      actual.set_output([&](std::string_view text) { actual_output += text; });
      auto expected = reference.run_source(source, "qbe-test.mml", RunMode::Eval);
      require(expected.ok() == (source != "1 / 0" && source != "true + 1"), "invalid fixture: " + source);
      auto result = backend.eval(actual, source, "qbe-test.mml");
      require(result.ok() == expected.ok(), source + (result.ok() ? "" : "\n" + actual.render(result.errors.front())));
      require(actual_output == reference_output, "effects: " + source);
      if (expected.ok()) require(show_value(*expected.value) == show_value(*result.value), "value: " + source);
      else {
        require(expected.errors.front().code == result.errors.front().code, "diagnostic code");
        require(reference.render(expected.errors.front()) == actual.render(result.errors.front()), "diagnostic span");
      }
      if (source != "true + 1") require(actual.interp().qbe_native_blocks > 0, "must execute QBE code");
      require(actual.interp().engine == saved && !actual.current_program(), "restore session context");
      require(!actual.interp().eval_depth && !actual.interp().call_depth, "restore runtime depths");
    }
    // Cache reuse across engines, tail transfers and rechecking an existing AST.
    session.interp().engine = ExecutionEngine::QBE;
    require(session.eval("val f x := x + 1\nval g := 41").is_ok(), "definitions");
    auto parsed = parse("f g", "reuse.mml");
    for (auto engine : {ExecutionEngine::QBE, ExecutionEngine::PortableXVM, ExecutionEngine::XVM, ExecutionEngine::QBE}) {
      session.interp().engine = engine;
      auto result = session.run_program(parsed.program, RunMode::Eval);
      require(result.ok() && result.value->i == 42, "engine cache isolation");
    }
    require(session.eval("val g x := x * 2").is_ok(), "rebind");
    auto composed = session.run_program(parsed.program, RunMode::Eval);
    require(composed.ok() && session.interp().apply(*composed.value, {Value::integer(20)}, {}).i == 42, "rechecked AST");
    auto closure = session.eval("fn x -> x + 1").unwrap();
    for (auto engine : {ExecutionEngine::QBE, ExecutionEngine::PortableXVM, ExecutionEngine::XVM, ExecutionEngine::QBE}) {
      session.interp().engine = engine;
      require(session.interp().apply(closure, {Value::integer(41)}, {}).i == 42, "reuse compiled closure across engines");
    }
    session.interp().xvm_native_limit = 1;
    bool limited = false;
    try { session.interp().apply(closure, {Value::integer(41)}, {}); }
    catch (const detail::MetaError &e) { limited = e.diag.code == "M0406"; }
    require(limited && !session.interp().eval_depth && !session.interp().call_depth, "cached native size limit");
    session.interp().xvm_native_limit = 1024 * 1024;
    require(session.interp().apply(closure, {Value::integer(41)}, {}).i == 42, "recover cached code after limit");
    Session restricted;
    restricted.install_primitive("Host.answer", "Unit -> Int", 1,
      [](PrimContext &, std::vector<Value> &) { return Value::integer(42); });
    error(backend.eval(restricted, "Host.answer(())"), "M0403");
    restricted.set_primitive_allowlist({"Host.answer"});
    require(backend.eval(restricted, "Host.answer(())").value->i == 42, "allowed host primitive");
    restricted.register_primitive("Host.nested", "Unit -> Int", 1,
      [](PrimContext &c, std::vector<Value> &) { return c.session.eval("20 + 22").unwrap(); });
    require(backend.eval(restricted, "Host.nested(()) + 0").value->i == 42, "retain active code across nested evaluation");
    Session bounded;
    bounded.interp().xvm_code_limit = 1;
    error(backend.eval(bounded, "42"), "M0406");
    bounded.interp().xvm_code_limit = 1000;
    bounded.interp().xvm_native_limit = 1;
    for (int i = 0; i < 2; ++i) error(backend.eval(bounded, "40 + 2"), "M0406");
    bounded.interp().xvm_native_limit = 1024 * 1024;
    bounded.interp().xvm_stack_limit = 1;
    error(backend.eval(bounded, "40 + 2"), "M0406");
    bounded.interp().xvm_stack_limit = 1000;
    bounded.interp().cancelled = [] { return true; };
    error(backend.eval(bounded, "40 + 2"), "M0410");
    bounded.interp().cancelled = {};
    require(backend.eval(bounded, "40 + 2").value->i == 42, "recover after limits");
    bounded.interp().xvm_native_limit = 0;
    error(backend.eval(bounded, "42"), "M0409");
    // Tool paths are executable names, never shell fragments. Failed compilation
    // must not fall back to another engine, poison the cache or leak temp files.
    struct Environment {
      std::string name, value; bool present;
      explicit Environment(std::string n) : name(std::move(n)) {
        auto p = std::getenv(name.c_str()); present = p; if (p) value = p;
      }
      ~Environment() { if (present) setenv(name.c_str(), value.c_str(), 1); else unsetenv(name.c_str()); }
    } qbe_env("METAML_QBE"), cc_env("METAML_QBE_CC");
    setenv("METAML_QBE", "/nonexistent/metaml-qbe;not-a-shell", 1);
    Session unavailable;
    auto missing = backend.eval(unavailable, "42");
    error(missing, "M0409");
    require(missing.errors.front().headline().find("cannot execute") != std::string::npos, "compiler diagnostic");
    setenv("METAML_QBE", "/bin/false", 1);
    error(backend.eval(unavailable, "42"), "M0409");
    if (qbe_env.present) setenv("METAML_QBE", qbe_env.value.c_str(), 1); else unsetenv("METAML_QBE");
    setenv("METAML_QBE_CC", "/nonexistent/metaml-cc", 1);
    error(backend.eval(unavailable, "42"), "M0409");
    if (cc_env.present) setenv("METAML_QBE_CC", cc_env.value.c_str(), 1); else unsetenv("METAML_QBE_CC");
    require(backend.eval(unavailable, "42").value->i == 42, "recover after compiler failure");
#else
    error(backend.eval(session, "42"), "M0409");
    require(session.interp().engine == saved, "restore engine after unavailable backend");
#endif
    std::cout << "MetaML QBE tests passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
