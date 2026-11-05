#include "../metaml-backends/XVM.hpp"
#include <iostream>
#include <stdexcept>

static void require(bool condition, const std::string &message)
{
  if (!condition) throw std::runtime_error(message);
}
static metaml::Value eval(metaml::Session &s, const std::string &source)
{
  auto r = s.eval(source);
  if (r.is_err()) throw std::runtime_error(source + "\n" + s.render(r.unwrap_err().front()));
  return r.unwrap();
}
int main(int argc, char **argv)
{
  try {
    require(argc == 2 || argc == 3, "stdlib path required; optional output directory");
    metaml::Session s;
    require(s.interp().engine == metaml::ExecutionEngine::XVM, "XVM is the default executor");
    metalib::install_toolchain(s);
    auto before = s.find_primitive("Chunk.of_source");
    metalib::install_toolchain(s);
    require(before == s.find_primitive("Chunk.of_source"), "idempotent installation");
    auto denied = s.eval("Chunk.of_source(\"42\")");
    require(denied.is_err(), "installation must not grant capabilities");
    s.allow_all_primitives();
    auto stdlib = s.load_stdlib(argv[1]);
    require(stdlib.is_ok(), "load stdlib");
    eval(s, "import \"ast\"\nimport \"system\"\nimport \"objfile\"\nimport \"xvm\"");
    auto check = [&](const std::string &expr) { require(eval(s, expr).i != 0, expr); };
    check("match Absyn.of_source \"val x := 42\" with | Ok tree -> Absyn.tag tree == \"Program\" | Err _ -> false");
    check("match Absyn.of_source \"val :=\" with | Err _ -> true | Ok _ -> false");
    check("match Absyn.current () with | Ok tree -> Absyn.tag tree == \"Program\" | Err _ -> false");
    check("match Absyn.current () with | Ok tree -> Absyn.Visitor.fold (fn n -> fn _ -> n + 1) 0 tree > 1 | Err _ -> false");
    eval(s, "module Host := System(Posix)\nmodule Object := Objfile(ELF)\nmodule CPU := Arch(Amd64)\nmodule VM := XVM(Amd64)");
    check("Host.name () == \"posix\"");
    check("CPU.word_bits () == 64");
    check("match Object.of_code \"amd64\" (CPU.return_int 42) with | Ok bytes -> Object.matches bytes | Err _ -> false");
    check("match Chunk.of_source \"40 + 2\" with | Ok chunk -> match VM.run chunk with | Ok value -> VM.show value == \"42\" | Err _ -> false | Err _ -> false");
    check("match Chunk.of_source \"40 + 2\" with | Ok chunk -> match Chunk.decode (Chunk.encode chunk) with | Ok restored -> Chunk.source restored == \"40 + 2\" | Err _ -> false | Err _ -> false");
    check("match Chunk.decode \"XVM\" with | Err _ -> true | Ok _ -> false");
    check("match Chunk.current () with | Ok chunk -> Chunk.filename chunk == \"<eval>\" | Err _ -> false");
    std::string output;
    s.set_output([&](std::string_view text) { output += text; });
    auto contracts = (std::filesystem::path(argv[1]).parent_path() / "tests/toolchain/contracts.mml").string();
    auto contract_source = metalib::absyn_detail::read_file(contracts);
    auto contract_result = s.run_source(contract_source, contracts, metaml::RunMode::Eval);
    require(contract_result.ok(), contract_result.ok() ? "" : s.render(contract_result.errors.front()));
    require(output == "rootleftrightleftrightrootroot", "visitor order and pruning");
    require(s.interp().eval_depth == 0 && s.interp().call_depth == 0, "nested error restores caller depths");
    require(s.current_program() == nullptr, "current source restored after contracts");
    check("match Absyn.of_file " + metaml::detail::quote_string(contracts) + " with | Ok tree -> Absyn.tag tree == \"Program\" | Err _ -> false");
    check("match Chunk.of_file " + metaml::detail::quote_string(contracts) + " with | Ok chunk -> Chunk.source chunk != \"\" | Err _ -> false");

    for (std::string format : {"elf", "pe", "coff", "macho"}) {
      for (std::string arch : {"amd64", "aarch64", "riscv64"}) {
        if (format == "macho" && arch == "riscv64") continue;
        auto bytes = metalib::objfile::of_code(format, arch, "\xc3");
        auto info = metalib::objfile::inspect(format, bytes);
        require(info.arch == arch && info.bits == 64 && info.sections > 0, "object round trip");
        bool failed = false;
        try { metalib::objfile::inspect(format, bytes.substr(0, bytes.size() / 2)); }
        catch (const std::exception &) { failed = true; }
        require(failed, "reject truncated " + format);
      }
    }
    auto native = metalib::arch::host();
    bool can_jit = false;
    if (native != metalib::arch::Target::Unsupported) {
      std::shared_ptr<metalib::xvm::Executable> code;
      try { code = std::make_shared<metalib::xvm::Executable>(metalib::arch::return_int(native, 0xfedcba9876543210ULL)); }
      catch (const std::runtime_error &) {} // Host executable-memory policy, not a guest failure.
      if (code) {
        require(code->entry<std::uint64_t (*)()>()() == 0xfedcba9876543210ULL, "native return literal");
        can_jit = true;
      }
    }

    const std::vector<std::string> programs{
      "40 + 2",
      "val make x := fn y -> x + y\nval add := make 40\nadd 2",
      "fn x -> x + 1",
      "val fact n := if n <= 1 then 1 else n * fact (n - 1)\nfact 6",
      "type Tree := | Tip Int | Fork Tree Tree\nval sum t := match t with | Tip n -> n | Fork a b -> sum a + sum b\nsum (Fork (Tip 20) (Tip 22))",
      "module type S := sig val x : Int end\nmodule F (M : S) := struct val y := M.x + 1 end\nmodule A := struct val x := 41 end\nmodule B := F(A)\nB.y",
      "val get r := r.x\nget {x := 42, y := true}",
      "[x * 2 for x <- [1, 2, 3]]",
      "import \"list\"\nList.reverse [1, 2, 3]",
      "val _ := IO.println(\"effect\")\n42",
      "1 / 0", "true + 1"
    };
    for (const auto &source : programs) {
      for (bool jit : {false, true}) {
        if (jit && !can_jit) continue;
        metaml::Session reference, actual;
        reference.interp().engine = metaml::ExecutionEngine::Tree;
        std::string reference_output, actual_output;
        reference.set_output([&](std::string_view text) { reference_output += text; });
        actual.set_output([&](std::string_view text) { actual_output += text; });
        reference.allow_all_primitives(); actual.allow_all_primitives();
        require(reference.load_stdlib(argv[1]).is_ok(), "reference stdlib");
        require(actual.load_stdlib(argv[1]).is_ok(), "actual stdlib");
        auto expected = reference.run_source(source, "test.mml", metaml::RunMode::Eval);
        require(expected.ok() == (source != "1 / 0" && source != "true + 1"), "invalid test program: " + source);
        metaml::backends::XVM backend;
        actual.interp().xvm_instructions = actual.interp().xvm_native_blocks = 0;
        auto result = backend.eval(actual, source, "test.mml", jit);
        require(expected.ok() == result.ok(), "backend status: " + source);
        require(reference_output == actual_output, "backend side effects");
        require(reference.interp().xvm_instructions == 0, "reference must use the tree engine");
        if (source != "true + 1") {
          require(actual.interp().xvm_instructions > 0, "compiled expression execution");
          require((actual.interp().xvm_native_blocks > 0) == jit, "native versus portable block execution");
        }
        if (expected.ok())
          require(metaml::show_value(expected.value.value_or(metaml::Value::unit())) ==
                  metaml::show_value(result.value.value_or(metaml::Value::unit())), "backend value: " + source);
        else require(expected.errors.front().code == result.errors.front().code, "backend diagnostic");
        require(actual.current_program() == nullptr, "restore current program");
      }
    }
    // A precompiled chunk must be reusable across sessions and repeated runs.
    auto chunk = metalib::xvm::compile("val x := 42\nx");
    metalib::xvm::VM vm;
    for (int i = 0; i < 3; ++i) require(vm.run(s, chunk).value->i == 42, "reusable chunk");
    auto encoded = metalib::xvm::encode(chunk);
    if (argc == 3) {
      auto path = (std::filesystem::path(argv[2]) / "toolchain-cpp.xvm").string();
      {
        std::ofstream f(path, std::ios::binary);
        f.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        f.close();
        require(bool(f), "write encoded test chunk");
      }
      auto restored = metalib::xvm::from_file(path);
      require(restored.source == chunk.source && restored.file == chunk.file, "binary chunk file round trip");
      check("match Chunk.of_file " + metaml::detail::quote_string(path) + " with | Ok chunk -> Chunk.source chunk == \"val x := 42\\nx\" | Err _ -> false");
    }
    for (std::size_t i = 0; i < 26; ++i) {
      bool failed = false;
      try { metalib::xvm::decode(encoded.substr(0, i)); }
      catch (const std::exception &) { failed = true; }
      require(failed, "truncated chunk");
    }
    bool failed = false;
    try { metalib::xvm::decode(encoded + "x"); }
    catch (const std::exception &) { failed = true; }
    require(failed, "trailing chunk bytes");
    encoded[24] = 99;
    failed = false;
    try { metalib::xvm::decode(encoded); }
    catch (const std::exception &) { failed = true; }
    require(failed, "reject unknown bytecode");
    encoded[24] = 1;
    for (std::size_t i = 8; i < 16; ++i) encoded[i] = '\xff';
    failed = false;
    try { metalib::xvm::decode(encoded); }
    catch (const std::exception &) { failed = true; }
    require(failed, "reject overflowing lengths");
    // A VM cannot use code execution to bypass the destination capability set.
    metaml::Session restricted;
    metalib::install_System(restricted);
    auto restricted_result = vm.run(restricted, metalib::xvm::compile("Posix.cwd(())"));
    require(!restricted_result.ok(), "preserve destination capabilities");
    require(restricted_result.errors.front().code == "M0403", "denied primitive diagnostic");
    auto other = native == metalib::arch::Target::Amd64 ? metalib::arch::Target::Aarch64 : metalib::arch::Target::Amd64;
    failed = false;
    try { metalib::xvm::VM(other).run(s, chunk, true); }
    catch (const std::exception &) { failed = true; }
    require(failed, "refuse cross-architecture JIT");
    std::int64_t composed_reference = 0;
    for (auto engine : {metaml::ExecutionEngine::Tree, metaml::ExecutionEngine::PortableXVM, metaml::ExecutionEngine::XVM}) {
      metaml::Session reuse;
      reuse.interp().engine = engine;
      eval(reuse, "val f x := x + 1\nval g := 41");
      auto parsed = metaml::parse("f g", "reuse.mml");
      require(parsed.ok(), "reusable AST parses");
      auto first = reuse.run_program(parsed.program, metaml::RunMode::Eval);
      require(first.ok() && first.value->i == 42, "initial application resolution");
      eval(reuse, "val g x := x * 2");
      auto second = reuse.run_program(parsed.program, metaml::RunMode::Eval);
      require(second.ok() && second.value.has_value(), "rechecked composition resolution");
      auto value = reuse.interp().apply(*second.value, {metaml::Value::integer(20)}, {}).i;
      if (engine == metaml::ExecutionEngine::Tree) composed_reference = value;
      else require(value == composed_reference, "invalidate bytecode after rechecking shared AST");
    }
    {
      metaml::Session bounded;
      bounded.interp().engine = metaml::ExecutionEngine::PortableXVM;
      bounded.interp().xvm_code_limit = 1;
      auto error = bounded.eval("42");
      require(error.is_err() && error.unwrap_err().front().code == "M0406", "compiled code budget");
      bounded.interp().xvm_code_limit = 1000;
      bounded.interp().xvm_stack_limit = 1;
      error = bounded.eval("1 + 2");
      require(error.is_err() && error.unwrap_err().front().code == "M0406", "operand budget");
      bounded.interp().xvm_stack_limit = 1000;
      bounded.interp().cancelled = [] { return true; };
      error = bounded.eval("42");
      require(error.is_err() && error.unwrap_err().front().code == "M0410", "VM cancellation");
      bounded.interp().cancelled = {};
      if (can_jit) {
        bounded.interp().engine = metaml::ExecutionEngine::NativeXVM;
        bounded.interp().xvm_native_limit = 1;
        for (int attempt = 0; attempt < 2; ++attempt) {
          error = bounded.eval("1 + 2");
          require(error.is_err() && error.unwrap_err().front().code == "M0406", "native code budget");
        }
        bounded.interp().xvm_native_limit = 1024 * 1024;
      }
      require(eval(bounded, "40 + 2").i == 42, "recovery after compiler/runtime limits");
    }
    {
      metaml::Session direct;
      direct.interp().engine = metaml::ExecutionEngine::PortableXVM;
      auto function = eval(direct, "fn x -> x + 1");
      direct.interp().xvm_code_limit = 1;
      bool limited = false;
      try { direct.interp().apply(function, {metaml::Value::integer(41)}, {}); }
      catch (const metaml::detail::MetaError &e) { limited = e.diag.code == "M0406"; }
      require(limited && !direct.interp().call_depth && !direct.interp().eval_depth, "direct closure compiler failure restores depths");
      direct.interp().xvm_code_limit = 1000;
      require(direct.interp().apply(function, {metaml::Value::integer(41)}, {}).i == 42, "direct closure remains reusable");
    }
    std::cout << "metalib toolchain tests passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
