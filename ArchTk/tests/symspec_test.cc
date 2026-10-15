#include "Symspec.hpp"
#include "DSLtk.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace symspec;

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
template<class T> T take(Result<T> result) {
    if (!result) { std::cerr << result.error().message << '\n'; std::exit(1); }
    return std::move(*result);
}
Json minimal_backend() {
    return boost::json::object{
        {"schema", 1}, {"arch", "test"},
        {"register_allocation", boost::json::object{}},
        {"instruction_selection", boost::json::object{}},
        {"instruction_scheduling", boost::json::object{}},
        {"instructions", boost::json::object{}}, {"dependencies", boost::json::object{}}
    };
}
void malformed_and_roundtrip() {
    for (auto source : {"", "{", "[]", "{\"schema\":2}", "{\"schema\":1,\"targets\":null}",
                        "{\"schema\":1,\"targets\":[1]}", "{\"schema\":1,\"targets\":[{}]}"})
        check(!Spec::from_json(source), "malformed schema/JSON accepted");
    auto value = minimal_backend();
    value.as_object()["unknown_extension"] = boost::json::object{
        {"null", nullptr}, {"empty_array", boost::json::array{}},
        {"empty_object", boost::json::object{}}, {"large", UINT64_MAX},
        {"negative", INT64_MIN}, {"unicode", "\xf0\x9f\x98\x80"},
        {"escaped", std::string("a\0\b\f\n\"\\]=]", 13)}
    };
    auto spec = take(Spec::from_value(value));
    auto serialized = take(spec.serialize());
    check(take(Spec::from_json(serialized)).json() == value, "JSON roundtrip lost data");
    auto module = take(spec.serialize(OutputFormat::backend_module));
    check(take(Spec::parse(module, InputFormat::backend_module)).json() == value, "Lua module roundtrip lost data");
    check(!Spec::parse(module + "print(1)", InputFormat::backend_module), "Lua data wrapper accepted executable suffix");
    check(!Spec::parse("return [=[{}", InputFormat::backend_module), "unterminated Lua module accepted");
    check(!Spec::from_json(serialized, ParseOptions{8, 256, true}), "byte limit not enforced");
    check(!Spec::from_json(serialized, ParseOptions{65536, 1, true}), "depth limit not enforced");
    check(!flag(&value), "non-boolean flag accepted");
    Json null;
    auto unknown = take(flag(&null));
    check(!unknown.has_value(), "null was converted to false");
    Json zero = 0;
    check(take(flag(&zero)).has_value() && !*take(flag(&zero)), "false flag not preserved");
    auto broken = value;
    broken.as_object().at("instructions").as_object()["bad"] = boost::json::object{
        {"Uses", boost::json::array{boost::json::object{{"kind", "def"}, {"def", "missing"}}}}
    };
    check(!Spec::from_value(broken), "dangling reference accepted");
    check(Spec::from_value(broken, ParseOptions{65536, 256, false}).has_value(), "reference-check opt-out failed");
    auto copy = spec;
    auto edited = take(copy.edited([](Json& document) -> Status {
        document.as_object()["changed"] = true;
        return std::monostate{};
    }));
    check(field(edited.json(), "changed") && !field(spec.json(), "changed"), "edit mutated original Spec");
    std::ostringstream output;
    Codec codec;
    dsl::Serializer encoder(codec);
    check(encoder.serialize(spec, output).has_value(), "DSLtk serialization adapter failed");
    std::istringstream input(output.str());
    dsl::Deserializer decoder(codec);
    check(take(decoder.deserialize<Spec>(input)).json() == value, "DSLtk deserialization adapter failed");
    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    check(!spec.write(failed), "failed output stream accepted");
    bool received = false;
    check(spec.feed([&](std::string_view json) -> Status {
        received = take(Spec::from_json(json)).json() == value;
        return std::monostate{};
    }).has_value() && received, "application receiver failed");
    check(!spec.feed([](std::string_view) -> Status { return fail(ErrorCode::io, "receiver rejected data"); }),
          "receiver failure not propagated");
    auto cycle = minimal_backend();
    cycle.as_object().at("dependencies").as_object()["a"] = boost::json::object{
        {"self", boost::json::object{{"kind", "def"}, {"def", "a"}}}
    };
    auto cyclic = take(Spec::from_value(cycle));
    check(take(cyclic.dependency_closure("a")).size() == 1, "dependency cycle did not terminate");
}

void integration(const Path& root, const Path& dsl_directory, const Path& temporary) {
    Repository repository(root);
    check(take(repository.architectures()).size() == 28, "source architecture discovery failed");
    auto architectures = take(repository.architectures(LoadMode::backend));
    check(architectures.size() == 28, "backend architecture discovery failed");
    check(!repository.load("../bad"), "invalid architecture key accepted");
    check(!Spec::load(temporary / "does-not-exist.json"), "missing input accepted");
    // Validate the complete bank, loading one architecture at a time.
    std::size_t instructions = 0;
    for (const auto& arch : architectures) {
        auto backend = take(repository.load(arch));
        check(backend.backend_only(), "direct backend loading evaluated an assembler spec");
        check(backend.architecture() == arch, "backend architecture mismatch");
        auto serialized = take(backend.serialize());
        check(take(Spec::from_json(serialized)).json() == backend.json(), "real backend roundtrip failed");
        instructions += backend.records(Group::instructions)->size();
    }
    check(instructions == 8393, "instruction records were lost");
    auto msp = take(repository.load("msp430"));
    auto add = take(msp.record("ADD16rr"));
    check(text(add.get("Constraints")) == "$src2 = $rd", "instruction tie lost");
    check(!take(flag(add.get("mayLoad"))).has_value(), "unresolved mayLoad was converted to false");
    check(!msp.instructions_for("ADD").empty(), "mnemonic query failed");
    auto closure = take(msp.dependency_closure("ADD16rr"));
    check(std::any_of(closure.begin(), closure.end(), [](auto r) { return r.id == "SR"; }), "implicit register missing from dependency closure");
    auto members = take(msp.register_members("GR16"));
    check(members.size() == 16 && members.front() == "R12", "register allocation order lost");
    std::array<std::string_view, 1> conditions{"has_frame_pointer"};
    auto reserved = take(msp.reserved_registers(conditions));
    check(std::find(reserved.begin(), reserved.end(), "R4") != reserved.end(), "conditional reservation failed");
    auto lanai = take(repository.load("lanai"));
    check(take(lanai.register_members("GPR")).size() > 32, "sequence/alias register expansion failed");
    check(integer(take(lanai.record("LanaiSchedModel")).get("IssueWidth")) == 1, "CPU schedule model query failed");
    std::ostringstream records;
    check(msp.write_records(records, Group::instructions).has_value(), "record export failed");
    std::istringstream rows(records.str());
    std::string row;
    std::size_t row_count = 0;
    while (std::getline(rows, row)) {
        boost::system::error_code ec;
        auto envelope = boost::json::parse(row, ec);
        check(!ec && text(field(envelope, "arch")) == "msp430", "invalid NDJSON record");
        ++row_count;
    }
    check(row_count == 343, "NDJSON record count mismatch");
    LoadOptions options;
    options.lua.dsl_directory = dsl_directory;
    for (auto arch : {"msp430", "lanai", "xcore"}) {
        auto full = take(repository.load(arch, LoadMode::source, options));
        check(!full.backend_only() && field(full.json(), "registers"), "Lua DSL failed to load assembler data");
        check(full.has_backend() && full.target() && full.architecture() == arch, "assembler accessors failed");
        const char* probe = arch == std::string("xcore") ? "ldw" : "add";
        check(!full.assembler_forms(probe).empty(), "assembler form query failed");
        check((full.sleigh() != nullptr) == (arch == std::string("msp430")), "sleigh accessor failed");
        check(full.coverage() && full.provenance(), "provenance/coverage accessors failed");
        auto backend = take(repository.load(arch));
        check(*full.backend() == backend.json(), "DSL and direct-backend data differ");
        check(full.save(temporary / (std::string(arch) + ".json")).has_value(), "saving JSON failed");
        check(take(Spec::load(temporary / (std::string(arch) + ".json"))).json() == full.json(), "file roundtrip failed");
    }
    auto script = take(msp.serialize(OutputFormat::lua_document));
    auto loaded = take(Spec::parse(script, InputFormat::lua_dsl, {}, temporary / "standalone.dsymspec"));
    check(loaded.json() == msp.json(), "standalone Lua serialization failed");
    LoadOptions callback;
    callback.evaluator = [&](std::string_view source, const Path& origin) -> Result<std::string> {
        check(source == "host DSL" && origin.filename() == "host.dsymspec", "host evaluator input mismatch");
        return msp.serialize();
    };
    check(take(Spec::parse("host DSL", InputFormat::lua_dsl, callback, "host.dsymspec")).json() == msp.json(), "host evaluator failed");
    check(!Spec::parse("error('intentional failure')", InputFormat::lua_dsl, {}, "test.dsymspec"), "Lua error not propagated");
    LoadOptions missing_runtime;
    missing_runtime.lua.executable = "definitely-no-such-symspec-lua";
    check(!Spec::parse("print('{}')", InputFormat::lua_dsl, missing_runtime, root / "missing.dsymspec"), "missing runtime not reported");
}
} // namespace

int main(int argc, char** argv) {
    check(argc == 4, "usage: symspec_test STDSPEC DSL_DIRECTORY TEMP_DIRECTORY");
    malformed_and_roundtrip();
    integration(argv[1], argv[2], argv[3]);
    std::cout << "Symspec: parsing, roundtrips, queries, bank validation, Lua and app integration passed\n";
}
