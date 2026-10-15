#include "MetaChain.hpp"
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <random>

static unsigned checks = 0;
// Sanitizer builds run a smaller deterministic sweep; CI default builds run more.
static const bool hostile_budget = std::getenv("METACHAIN_HOSTILE_SWEEP") != nullptr;
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ':' << __LINE__ << ": " #__VA_ARGS__ "\n"; std::exit(1); } } while (false)
std::string_view metachain_other_translation_unit();

static void parser_tests() {
    parsetk::Registry r;
    r.add(parsetk::Lexeme<"@INT">(parsetk::Pattern::Repo::Integer));
    r.add(parsetk::Keyword<"$LET">(parsetk::Pattern::Exact("let")));
    r.add(parsetk::ProductionRule<"#List">(parsetk::Production::New("'[' (@INT (',' @INT)*)? ']'")));
    r.add(parsetk::ProductionRule<"#Start">(parsetk::Production::New("($LET @INT | #List) !'.'")));
    parsetk::Parser p(r);
    CHECK(p.grammar_error().empty());
    std::vector<parsetk::Event> events;
    p.subscribe([&](const auto& e) { events.push_back(e); });
    CHECK(p.parse("[1, 2,3]", "#Start"));
    CHECK(events.front().kind == parsetk::EventKind::RuleBegin);
    CHECK(events.back().kind == parsetk::EventKind::RuleEnd);
    events.clear(); CHECK(!p.parse("letdown 2", "#Start")); CHECK(events.size() == 1);
    CHECK(p.parse("let 2", "#Start")); CHECK(p.parse("[]", "#Start"));
    CHECK(!p.parse("[1,]", "#Start")); CHECK(!p.parse("[1].", "#Start"));
    r.add(parsetk::ProductionRule<"#Right">(parsetk::Production::New("'a' #Right?")));
    CHECK(parsetk::Parser(r).parse("aaaa", "#Right"));
    r.add(parsetk::ProductionRule<"#Left">(parsetk::Production::New("#Left 'a' | 'a'")));
    CHECK(!parsetk::Parser(r).parse("a", "#Left"));
    r.add(parsetk::ProductionRule<"#EmptyLoop">(parsetk::Production::New("('')*")));
    CHECK(!parsetk::Parser(r).parse("", "#EmptyLoop"));
    r.add(parsetk::ProductionRule<"#Unknown">(parsetk::Production::New("@MISSING")));
    CHECK(!parsetk::Parser(r).grammar_error().empty());
    parsetk::Registry punct;
    punct.add(parsetk::ProductionRule<"#P">(parsetk::Production::New("'(' 'name' ')'")));
    CHECK(parsetk::Parser(punct).parse("(name)","#P"));
    parsetk::Options options; options.max_operations=1;
    CHECK(!parsetk::Parser(punct,options).parse("(name)","#P"));
    parsetk::Registry bad;
    bad.add(parsetk::Lexeme<"@BAD">(parsetk::PatternSpec::regex("[")));
    CHECK(!parsetk::Parser(bad).grammar_error().empty());
    parsetk::Registry escaped;
    escaped.add(parsetk::ProductionRule<"#S">(parsetk::Production::New("'a\\nb'")));
    CHECK(parsetk::Parser(escaped).parse("a\nb","#S"));
    parsetk::Registry transaction;
    transaction.add(parsetk::ProductionRule<"#S">(parsetk::Production::New("('a' 'b' | 'a' 'c')")));
    parsetk::Parser committed(transaction); std::vector<std::string> tokens;
    committed.subscribe([&](const auto& e) { if (e.kind==parsetk::EventKind::Token) tokens.push_back(e.text); });
    CHECK(committed.parse("ac","#S")); CHECK(tokens==std::vector<std::string>({"a","c"}));
    committed.subscribe([&](const auto&) { CHECK(!committed.parse("ac","#S")); });
    CHECK(committed.parse("ac","#S"));
}
static void rewrite_tests() {
    using namespace rewrite;
    Rule zero("zero",node("add",{var("x"),atom("0")}),var("x"));
    System s; s.add(zero);
    auto r=s.normalize(node("add",{node("add",{atom("3"),atom("0")}),atom("0")}));
    CHECK(r.normalized && r.steps==2 && r.term==atom("3"));
    Substitution subst{{"keep",atom("x")}};
    CHECK(!match(node("p",{var("x"),var("x")}),node("p",{atom("1"),atom("2")}),subst));
    CHECK(subst.size()==1);
    bool threw=false;
    try { Rule invalid("bad",node("x"),var("unbound")); } catch(const std::invalid_argument&) { threw=true; }
    CHECK(threw);
    Options o; o.direction=Direction::TopDown; o.max_steps=4;
    System growth(o); growth.add(Rule("grow",node("f",{var("x")}),node("f",{node("f",{var("x")})})));
    auto g=growth.normalize(node("f",{atom("x")})); CHECK(g.step_limit_reached && g.steps==4);
    System cycle; cycle.add(Rule("a-b",node("a"),node("b"))); cycle.add(Rule("b-a",node("b"),node("a")));
    CHECK(cycle.normalize(node("a")).cycle_detected);
    System identity; identity.add(Rule("id",node("a"),node("a"))); CHECK(identity.normalize(node("a")).normalized);
    o.max_steps=0; System empty(o); CHECK(empty.normalize(atom("x")).normalized);
    CHECK(node("x").to_string()!=atom("x").to_string());
}
static void il_tests() {
    const auto source="const r0, 1\nconst r1, 6\nconst r2, 1\nloop: mul r0, r0, r1\nsub r1, r1, r2\njnz r1, loop\nprint r0\nhalt r0\n";
    auto p=metail::parse(source); CHECK(p);
    auto r=metail::run(p.program); CHECK(r && r.value==720 && r.output==std::vector<std::int64_t>{720});
    auto encoded=metail::encode(p.program); auto decoded=metail::decode(encoded); CHECK(decoded);
    CHECK(decoded.program.code==p.program.code);
    auto roundtrip=metail::parse(metail::emit(p.program)); CHECK(roundtrip && roundtrip.program.code==p.program.code);
    encoded.push_back(0); CHECK(!metail::decode(encoded));
    encoded.pop_back(); encoded[12]=255; CHECK(!metail::decode(encoded));
    CHECK(!metail::parse("jmp missing\n")); CHECK(!metail::parse("const r32, 1\n")); CHECK(!metail::parse("const r0, 18446744073709551615\n"));
    auto bad=metail::parse("const r0, 1\nconst r1, 0\ndiv r0, r0, r1\nhalt r0\n"); CHECK(bad && !metail::run(bad.program));
    auto self=metail::parse("again: jmp again\n"); CHECK(self);
    metavm::VMOptions o; o.max_steps=5;
    CHECK(metail::run(self.program,o).status==metavm::Status::StepLimit);
    metail::VM vm(o); vm.load(self.program); CHECK(vm.run()==metavm::Status::StepLimit && vm.pc()==0 && vm.steps()==5);
    o.collect_trace=true; o.hot_threshold=1; o.max_trace_length=10; o.max_steps=10;
    metail::VM traced(o); traced.load(self.program); traced.run(); CHECK(!traced.traces().empty()); CHECK(traced.traces()[0].pcs==std::vector<std::size_t>{0});
    auto path=metail::parse("const r0, 1\nconst r1, 2\njmp 0\n"); CHECK(path);
    metail::VM path_vm(o); path_vm.load(path.program); path_vm.run();
    CHECK(path_vm.traces().size()==1 && path_vm.traces()[0].pcs==std::vector<std::size_t>({0,1,2}));
    struct Backend : metavm::JitBackend<metail::VM,metail::Instruction> {
        bool invalid_pc=false, fail_compile=false;
        std::unique_ptr<metavm::CompiledTrace> compile(metail::VM&,const metail::Program&,const metavm::Trace&,metavm::Diagnostic&) override {
            if (fail_compile) throw std::runtime_error("backend compiler failure");
            return std::make_unique<metavm::CompiledTrace>();
        }
        bool execute(metail::VM& vm,const metavm::CompiledTrace&,std::size_t& next,metavm::Diagnostic&) override {
            next=vm.program().size()+(invalid_pc?1:0); return true;
        }
    } backend;
    CHECK(path_vm.compile_trace(backend,0)); CHECK(!path_vm.compile_trace(backend,99));
    CHECK(!path_vm.try_execute_compiled(backend)); // step-limited VM must not re-enter native code
    path_vm.options().max_steps=100;
    // Load clears trace caches; record again and execute before reaching limit.
    path_vm.load(path.program); for(unsigned n=0;n<6;++n) path_vm.step();
    CHECK(path_vm.compile_trace(backend,0)); CHECK(path_vm.try_execute_compiled(backend)); CHECK(path_vm.status()==metavm::Status::Halted);
    path_vm.load(path.program); for(unsigned n=0;n<6;++n) path_vm.step();
    backend.fail_compile=true; CHECK(!path_vm.compile_trace(backend,0)); backend.fail_compile=false;
    CHECK(path_vm.compile_trace(backend,0)); backend.invalid_pc=true;
    CHECK(!path_vm.try_execute_compiled(backend)); CHECK(path_vm.status()==metavm::Status::Trapped);
    auto simple=metail::parse("const r0, 40\nconst r1, 2\nadd r0, r0, r1\nhalt r0\n"); CHECK(simple);
    auto opt=metail::optimize(simple.program); CHECK(opt && opt.program.code[2].op==metail::Op::Const); CHECK(metail::run(opt.program).value==42);
    CHECK(metail::evaluate(metail::Op::Add,INT64_MAX,1)==INT64_MIN);
    CHECK(!metail::evaluate(metail::Op::Div,INT64_MIN,-1));
    CHECK(metail::evaluate(metail::Op::Shr,-1,63)==1);
    auto call=metail::parse("call f\nhalt r0\nf: const r0, 42\nret\n"); CHECK(call && metail::run(call.program).value==42);
    auto under=metail::parse("pop r0\nhalt r0\n"); CHECK(under && !metail::run(under.program));
    auto output=metail::parse("const r0, 7\nprint r0\nhalt r0\n"); CHECK(output);
    metail::VM output_vm; std::vector<std::int64_t> captured;
    output_vm.set_output_sink([&](const auto& value) { captured.push_back(value.as_integer()); });
    output_vm.load(output.program); CHECK(output_vm.run()==metavm::Status::Halted && captured==std::vector<std::int64_t>{7});
    std::mt19937_64 random(123);
    for(unsigned n=0;n<300;++n) {
        auto a=std::bit_cast<std::int64_t>(random()), b=std::bit_cast<std::int64_t>(random());
        auto op=static_cast<metail::Op>(2+random()%13); if(!metail::binary(op)) continue;
        metail::Builder builder; builder.constant(0,a).constant(1,b).binary(op,2,0,1).halt(2);
        auto program=builder.finish(); CHECK(program);
        auto optimized=metail::optimize(program.program); CHECK(optimized);
        auto before=metail::run(program.program), after=metail::run(optimized.program);
        CHECK(before.status==after.status && before.value==after.value);
    }
}
static void assembler_tests() {
    asmtk::Assembler a;
    auto r=a.assemble("mov rax, 60\nmov rdi, 42\nsyscall\n"); CHECK(r);
    CHECK(r.bytes==std::vector<std::uint8_t>({0x48,0xb8,60,0,0,0,0,0,0,0,0x48,0xbf,42,0,0,0,0,0,0,0,0x0f,0x05}));
    auto strings=a.assemble(".ascii \"a;#b:c\", \"\\x41\"\n.asciz \"x\"\n"); CHECK(strings);
    CHECK(strings.bytes==std::vector<std::uint8_t>({'a',';','#','b',':','c','A','x',0}));
    CHECK(!a.assemble(".byte 256\n")); CHECK(!a.assemble(".byte -129\n")); CHECK(a.assemble(".byte -128, 255\n"));
    CHECK(!a.assemble(".ascii \"oops\\\"\n")); CHECK(!a.assemble(".unknown 1\n"));
    auto j=a.assemble("jmp forward\n.byte 0\nforward: ret\n"); CHECK(j && j.bytes[1]==1);
    CHECK(!a.assemble("jmp unknown\n"));
    CHECK(!a.assemble("mov al, missing\n")); CHECK(!a.assemble("mov rax, [r8\n"));
    asmtk::Options o; o.max_output_bytes=1; CHECK(!asmtk::Assembler(o).assemble(".asciz \"x\"\n"));
    CHECK(!asmtk::Assembler(o).assemble("mov rax, 1\n"));
    auto object=a.assemble_object(".global _start\n_start: mov rax, 60\nmov rdi, 42\nsyscall\n"); CHECK(object);
    auto image=ld_tk::Linker().link({object.bytes});
    if(!image) for(const auto& e:image.diagnostics) std::cerr << e.message << '\n';
    CHECK(image);
    ld_tk::detail::Ehdr header{}; CHECK(ld_tk::detail::read_at(image.image,0,header)); CHECK(header.type==2 && header.phnum==3);
    for(unsigned n=0;n<header.phnum;++n) {
        ld_tk::detail::Phdr ph{}; CHECK(ld_tk::detail::read_at(image.image,header.phoff+n*sizeof(ph),ph)); CHECK((ph.flags&3)!=3);
    }
    CHECK(!ld_tk::Linker().link({{1,2,3}}));
    CHECK(!ld_tk::Linker().link({object.bytes,object.bytes}));
    auto aligned_object=object.bytes;
    ld_tk::detail::Ehdr object_header{}; CHECK(ld_tk::detail::read_at(aligned_object,0,object_header));
    ld_tk::detail::Shdr text_section{}; CHECK(ld_tk::detail::read_at(aligned_object,object_header.shoff+sizeof(text_section),text_section));
    text_section.addralign=65536;
    std::memcpy(aligned_object.data()+object_header.shoff+sizeof(text_section),&text_section,sizeof(text_section));
    ld_tk::Options alignment_options; alignment_options.image_base=0x401000;
    auto aligned_image=ld_tk::Linker(alignment_options).link({aligned_object}); CHECK(aligned_image);
    CHECK(ld_tk::detail::read_at(aligned_image.image,0,header)); CHECK(header.entry%65536==0);
    auto ext=a.assemble_object(".global _start\n.extern exit\n_start: call exit\n"); CHECK(ext);
    auto def=a.assemble_object(".global exit\nexit: mov rax, 60\nmov rdi, 7\nsyscall\n"); CHECK(def);
    CHECK(ld_tk::Linker().link({ext.bytes,def.bytes})); CHECK(!ld_tk::Linker().link({ext.bytes}));
    auto corrupt=object.bytes; corrupt[5]=2; CHECK(!ld_tk::Linker().link({corrupt}));
    CHECK(ld_tk::detail::relocated(UINT64_MAX,-4,UINT64_MAX-10,true,4,true)==6);
    CHECK(!ld_tk::detail::relocated(0,INT64_MIN,0,true,4,true));
    for(std::size_t n=0;n<object.bytes.size();n+=7) {
        auto truncated=object.bytes; truncated.resize(n); CHECK(!ld_tk::Linker().link({truncated}));
    }
    std::mt19937 random(2026);
    ld_tk::Options fuzz_options; fuzz_options.max_output_size=1u<<20;
    for (unsigned n=0;n<400;++n) {
        auto mutated=object.bytes;
        for(unsigned j=0;j<4;++j) mutated[random()%mutated.size()]=static_cast<std::uint8_t>(random());
        auto fuzzed=ld_tk::Linker(fuzz_options).link({mutated});
        CHECK(!fuzzed || fuzzed.image.size()<=fuzz_options.max_output_size);
    }
}
// Deterministic robustness sweep: arbitrary bytes must produce diagnostics,
// never crashes, UB, or unbounded output from any text or binary front end.
static void hostile_input_sweep() {
    const unsigned hostile_rounds = hostile_budget ? 20000u : 2000u;
    std::mt19937 random(0x5eed1234);
    const std::string seeds[] = {
        "const r0, 1\nhalt r0\n", "[1, 2]", "add($x, 0) => $x",
        "zero: add($x, 0) => $x\n", ".global _start\n_start: mov rax, 60\nsyscall\n",
        "lexeme @N = \"[0-9]+\"\nrule #S = @N\n", "const r0, -9223372036854775808\n",
        "rule #S = ('a' / 'b')* 'c'", ".align 8\n.byte 1\n", "push r1\nret\n"};
    std::vector<std::uint8_t> mcil_seed = metail::encode(metail::parse("const r0, 5\nhalt r0\n").program);
    asmtk::Assembler assembler;
    parsetk::Registry registry;
    registry.add(parsetk::Lexeme<"@N">(parsetk::Pattern::Repo::Integer));
    registry.add(parsetk::ProductionRule<"#S">(parsetk::Production::New("@N (',' @N)*")));
    parsetk::Parser sweep_parser(registry);
    metachain::Options bounded; bounded.vm.max_steps = 512; bounded.il.max_instructions = 4096;
    metachain::Toolchain toolchain(bounded);
    metavm::VMOptions short_run; short_run.max_steps = 512;
    for (unsigned round = 0; round < hostile_rounds; ++round) {
        std::string input = seeds[random() % (sizeof(seeds)/sizeof(*seeds))];
        if (!input.empty()) {
            const unsigned mutations = random() % 8;
            for (unsigned n = 0; n < mutations; ++n) {
                const auto at = static_cast<std::size_t>(random() % input.size());
                switch (random() % 3) {
                case 0: input[at] = static_cast<char>(random() & 0x7f); break;
                case 1: input.erase(at, 1 + random() % 4); break;
                default: input.insert(at, 1, static_cast<char>(32 + random() % 95)); break;
                }
                if (input.empty()) break;
            }
        }
        CHECK(metail::parse(input).program.code.size() <= bounded.il.max_instructions);
        CHECK(metail::run(metail::parse(input).program, short_run, bounded.il).diagnostics.size() < 10000);
        CHECK(assembler.assemble(input).bytes.size() <= assembler.options().max_output_bytes);
        CHECK(assembler.assemble_object(input).bytes.size() <= assembler.options().max_output_bytes);
        (void)!metachain::text::parse_term(input);
        (void)!metachain::text::parse_rules(input);
        (void)!sweep_parser.parse(input, "#S");
        (void)!toolchain.parse(input, registry, "#S");
        (void)!toolchain.run(input);
        (void)!toolchain.rewrite(input, "zero: add($x, 0) => $x\n");
    }
    for (unsigned round = 0; round < hostile_rounds / 2; ++round) {
        auto bytes = mcil_seed;
        const unsigned mutations = random() % 5;
        for (unsigned n = 0; n < mutations; ++n) bytes[random() % bytes.size()] = static_cast<std::uint8_t>(random());
        CHECK(metail::decode(bytes).program.code.size() <= metail::Limits{}.max_instructions);
    }
}
static void facade_io_tests() {
    metachain::Toolchain tc;
    auto object = tc.assemble(".global _start\n_start: mov rax, 60\nmov rdi, 11\nsyscall\n", true);
    CHECK(object);
    auto directory = std::filesystem::temp_directory_path() / "metachain-io-test";
    std::filesystem::create_directories(directory);
    struct Cleanup { std::filesystem::path d; ~Cleanup() { std::error_code e; std::filesystem::remove_all(d,e); } } cleanup{directory};
    const auto object_path = directory / "start.o";
    CHECK(ld_tk::Linker::write_file(object_path.string(), *object));
    auto linked = tc.link_files({object_path.string()});
    CHECK(linked && linked->size() > 64);
    auto missing = tc.link_files({(directory / "absent.o").string()});
    CHECK(!missing && missing.diagnostics.size()==1 && missing.diagnostics[0].stage==metachain::Stage::IO);
    auto empty = tc.link_files({}); CHECK(!empty && empty.diagnostics[0].stage==metachain::Stage::IO);
    auto duplicate = tc.link_files({object_path.string(),object_path.string()});
    CHECK(!duplicate && duplicate.diagnostics[0].stage==metachain::Stage::Link);
    CHECK(!ld_tk::Linker::write_file(directory.string(), std::vector<std::uint8_t>{1,2,3}));
    auto direct = std::string_view("const r0, 9\nhalt r0\n") | tc.compile_stage() | tc.executable_stage();
    CHECK(direct);
    auto assembly_text = std::string_view("const r0, 9\nhalt r0\n") | tc.compile_stage() | tc.native_stage();
    CHECK(assembly_text && assembly_text->find(".text") != std::string::npos);
    auto term = tc.parse_term("add(1, 2)"); CHECK(term && term->is_node());
    CHECK(!tc.parse_term("add(1, 2")); CHECK(!tc.parse_rules("no arrow here\n"));
    auto rules = tc.parse_rules("zero: add($x, 0) => $x\n"); CHECK(rules && rules->size()==1);
}
static void facade_tests() {
    CHECK(metachain_other_translation_unit()==metachain::version);
    metachain::Toolchain tc;
    auto pipeline=std::string_view("const r0, 42\nhalt r0\n") | tc.compile_stage() | tc.run_stage(); CHECK(pipeline && pipeline->value==42);
    auto failed=std::string_view("invalid\n") | tc.compile_stage() | tc.run_stage(); CHECK(!failed && !failed.diagnostics.empty());
    using namespace metachain::native;
    Script s; auto a=reg(0), b=reg(1);
    s.set(a,6).set(b,7).set(a,a*b).print(a).halt(a);
    auto p=s.finish(); CHECK(p);
    auto r=tc.run(*p); CHECK(r && r->value==42);
    auto executable=tc.executable(*p);
    if(!executable) for(const auto& e:executable.diagnostics) std::cerr << e.message << '\n';
    CHECK(executable);
    CHECK(!metachain::evaluate("nope", "")); CHECK(metachain::evaluate("run","const r0, 42\nhalt r0\n").text=="result 42\n");
    auto rewritten=tc.rewrite("add(add(3, 0), 0)","zero: add($x, 0) => $x\n"); CHECK(rewritten && *rewritten==rewrite::atom("3"));
    CHECK(!tc.rewrite("add(1, 2)","bad: add($x, 0) => $missing\n"));
    auto term=metachain::text::parse_term("node(\"a\\x00b\", leaf())"); CHECK(term);
    CHECK(metachain::text::parse_term(term.term->to_string()));
    auto parsed=metachain::evaluate("parse","lexeme @INT = \"[0-9]+\"\nrule #S = @INT (',' @INT)*\n%%\n1,2,3\n");
    CHECK(parsed && parsed.text=="@INT \"1\"\n, \",\"\n@INT \"2\"\n, \",\"\n@INT \"3\"\n");
    CHECK(metachain::evaluate("rewrite","zero: add($x, 0) => $x\n%%\nadd(42, 0)\n").text=="42\n");
    const auto ast=dsl::node<"add">(dsl::leaf<"integer">(42),dsl::node<"empty">());
    auto restored=metachain::term_to_ast(metachain::ast_to_term(ast)); CHECK(restored && *restored==ast);
    CHECK(!metachain::term_to_ast(rewrite::atom("invalid")));
    auto deep = dsl::ASTNode("n", std::vector<dsl::ASTNode>{});
    for (unsigned n=0;n<300;++n) deep = dsl::ASTNode("n", std::vector<dsl::ASTNode>{deep});
    bool depth_guard=false;
    try { metachain::ast_to_term(deep); } catch (const std::invalid_argument&) { depth_guard=true; }
    CHECK(depth_guard);
    CHECK(metachain::ast_to_term(deep, 400).is_node());
    // Left-deep chains reuse one temporary per binary node; the bound that
    // actually protects the C++ stack is the expression depth limit.
    Expr chain = reg(0);
    for (unsigned n=0;n<300;++n) chain = chain + Expr(1);
    Script overflow; overflow.set(reg(0), chain);
    auto failed_overflow = overflow.finish();
    CHECK(!failed_overflow && !failed_overflow.diagnostics.empty());
    Script garbage; garbage.set(reg(0), Expr(dsl::leaf<"unknown">(1)));
    auto rejected_form = garbage.finish(); CHECK(!rejected_form);
    metachain::Options tiny; tiny.il.max_instructions = 2;
    auto rejected_by_limit = metachain::Toolchain(tiny).run("const r0, 1\nconst r1, 2\nconst r2, 3\nhalt r0\n");
    CHECK(!rejected_by_limit && !rejected_by_limit.diagnostics.empty());
}
int main() {
    parser_tests(); rewrite_tests(); il_tests(); assembler_tests(); facade_tests();
    facade_io_tests(); hostile_input_sweep();
    std::cout << checks << " checks passed\n";
}
