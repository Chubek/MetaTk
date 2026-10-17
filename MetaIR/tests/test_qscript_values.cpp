#include "qscript/include/QScript-Runtime.h"
#include "qscript/include/QScript-Generated.h"
#include "qscript/src/Runtime.h"
#include <cstdlib>
#include <iostream>

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __LINE__ << ": " #__VA_ARGS__ "\n"; std::exit(1); } } while (false)
extern "C" int qscript_values_c(void);

static void generated_runtime_tests() {
    using qscript::detail::Builtin;
    using qscript::detail::builtin_id;
    auto* context = qs_create(); CHECK(context);
    std::string source = "grammar G { token TOP { 'a' } }";
    qs_runtime_string strings[] = {{source.data(), source.size()}, {"a", 1}};
    const char* types[] = {"str"};
    qs_runtime_config config{strings, 2, types, 1, nullptr, 0, 100, 100000};
    auto* runtime = qs_runtime_create(context, &config); CHECK(runtime);
    source.assign(source.size(), '!'); // runtime owns its table snapshot
    int64_t regs[32] = {}, grammar = 0, input = 0, match = 0, result = 0;
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::StrLiteral), regs, 32, &grammar) == QS_OK);
    regs[0] = grammar;
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::CompileGrammar), regs, 32, &grammar) == QS_OK);
    regs[0] = 1;
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::StrLiteral), regs, 32, &input) == QS_OK);
    regs[0] = grammar; regs[1] = input;
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::GrammarParse), regs, 32, &match) == QS_OK);
    regs[0] = match;
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::MatchOk), regs, 32, &result) == QS_OK && result == 1);
    CHECK(qs_runtime_dispatch(context, runtime, 0, regs, 32, &result) == QS_INVALID_ARGUMENT && result == 0);
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::MatchOk), regs, 1, &result) == QS_INVALID_ARGUMENT);
    regs[0] = 0;
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::MatchOk), regs, 32, &result) == QS_ERROR);
    auto* other = qs_create(); CHECK(other);
    CHECK(qs_runtime_dispatch(other, runtime, builtin_id(Builtin::MatchOk), regs, 32, &result) == QS_INVALID_ARGUMENT);
    qs_destroy(other);
    qs_runtime_destroy(runtime);
    config.types = nullptr;
    CHECK(!qs_runtime_create(context, &config));
    config.types = types; config.string_count = SIZE_MAX;
    CHECK(!qs_runtime_create(context, &config));
    config.string_count = 2; config.max_heap_bytes = 1;
    runtime = qs_runtime_create(context, &config); CHECK(runtime);
    CHECK(qs_runtime_dispatch(context, runtime, builtin_id(Builtin::StrLiteral), regs, 32, &result) == QS_ERROR);
    CHECK(std::string(qs_error(context)).find("heap size limit exceeded") != std::string::npos);
    qs_runtime_destroy(runtime); qs_destroy(context);
}

int main() {
    CHECK(qscript_values_c());
    generated_runtime_tests();
    using qscript::Value;
    const auto unit = qscript::compile_unit(R"QS(
        fn decorate(s: str): str { return "[" ~ s ~ "]"; }
        fn printed(s: str): str { print 1; print s; print 2; return s; }
        fn alias(a: list<int>, b: list<int>): list<int> { a.push(7); return b; }
        fn nested(a: list<list<str>>): list<list<str>> { a[0].push("new"); return a; }
        fn parse(g: grammar, s: str): match { return g.parse(s); }
        fn child(m: match): match { return m.child("word"); }
        fn text(m: match): str { return m.text(); }
        fn failed(m: match): int { return !m.ok(); }
        fn extend(g: grammar): grammar { return g.extend("token word { <[A..Z]>+ }"); }
        fn bad(): str { assert 0; return "bad"; }
        fn forever(): str { loop {} return "bad"; }
        die("top-level statements must not run");
    )QS");
    CHECK(unit);
    for (bool jit : {false, true}) {
        qscript::Options options; options.jit = jit;
        std::vector<std::string> lines;
        options.text_output = [&](std::string_view line) { lines.emplace_back(line); };
        options.toolchain.jit.mode = metachain::jit::Mode::Method;
        options.toolchain.jit.method_threshold = 1;
        const auto call = [&](std::string_view name, std::vector<Value> args) {
            auto result = qscript::call_value(*unit, name, args, options);
            if (!result) for (const auto& d : result.diagnostics) std::cerr << d.message << '\n';
            CHECK(result);
            if (result->value.kind() != Value::Kind::Int) CHECK(result->execution.value == 0);
            return std::move(result->value);
        };
        auto text = call("decorate", {Value(std::string("a\0b", 3))});
        CHECK(text.as_string() && *text.as_string() == std::string("[a\0b]", 5));
        CHECK(*call("printed", {Value(std::string("x"))}).as_string() == "x");
        CHECK(lines == std::vector<std::string>({"1", "x", "2"}));
        auto empty = Value::list("int", {}); CHECK(empty);
        auto aliased = call("alias", {*empty, *empty});
        CHECK(aliased.type() == "list<int>" && aliased.as_list()->size() == 1);
        CHECK((*aliased.as_list())[0].as_integer() == 7 && empty->as_list()->empty());
        CHECK(call("alias", {*empty, *empty}).as_list()->size() == 1);
        auto strings = Value::list("str", {Value(std::string("old"))}); CHECK(strings);
        auto nested = Value::list("list<str>", {*strings}); CHECK(nested);
        auto result = call("nested", {*nested});
        CHECK(result.as_list()->front().as_list()->size() == 2 && strings->as_list()->size() == 1);
        auto grammar = Value::compile_grammar("grammar Words { token TOP { <word> } token word { <[a..z]>+ } }"); CHECK(grammar);
        auto match = call("parse", {*grammar, Value(std::string("hello"))});
        CHECK(match.as_match() && match.as_match()->text == "hello");
        auto child = call("child", {match});
        CHECK(child.as_match() && child.as_match()->name == "word");
        match = Value(); grammar = {};
        CHECK(call("text", {child}).as_string() && *call("text", {child}).as_string() == "hello");
        auto copied = Value::from_match(*child.as_match()); CHECK(copied);
        CHECK(*call("text", {*copied}).as_string() == "hello");
        auto base = Value::compile_grammar("grammar G { token TOP { <word> } token word { 'a' } }"); CHECK(base);
        auto derived = call("extend", {*base});
        auto derived_match = call("parse", {derived, Value(std::string("ABC"))});
        CHECK(derived_match.as_match());
        auto failure = call("parse", {derived, Value(std::string("123"))});
        CHECK(!failure.as_match() && !failure.match_error().empty());
        CHECK(call("failed", {failure}).as_integer() == 1);
        std::vector<Value> wrong{Value(std::int64_t(2))};
        auto bad_type = qscript::call_value(*unit, "decorate", wrong, options);
        CHECK(!bad_type && bad_type.diagnostics[0].message.find("expects str, got int") != std::string::npos);
        CHECK(!qscript::call_value(*unit, "missing", {}, options));
        CHECK(!qscript::call_value(*unit, "decorate", {}, options));
        auto trap = qscript::call_value(*unit, "bad", {}, options);
        CHECK(!trap && trap.value && trap.diagnostics[0].message == "assertion failed");
        options.toolchain.vm.max_steps = 20;
        auto limited = qscript::call_value(*unit, "forever", {}, options);
        CHECK(!limited && limited.value && limited->execution.status == metavm::Status::StepLimit);
        options.max_heap_bytes = 1;
        auto heap = qscript::call_value(*unit, "decorate", std::vector<Value>{Value(std::string("large"))}, options);
        CHECK(!heap && heap.diagnostics[0].message == "heap size limit exceeded");
    }
    CHECK(!Value::list("str", {Value(std::int64_t(1))}));
    CHECK(!Value::compile_grammar("not a grammar"));
    std::string deep;
    for (unsigned i = 0; i < 1000; ++i) deep += "list<";
    deep += "int"; deep.append(1000, '>');
    CHECK(!Value::list(deep, {}));
    qs_match tree;
    for (unsigned i = 0; i < 300; ++i) { qs_match parent; parent.children.push_back(std::move(tree)); tree = std::move(parent); }
    CHECK(!Value::from_match(tree));
    std::cout << "Typed QScript values: C/C++, VM/JIT, lifetimes, aliases and errors passed\n";
}
