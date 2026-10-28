#pragma once

#include <MetaIL.h>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace juniper {

struct Diagnostic {
    std::string code, file, message;
    std::size_t line = 0, column = 0;
};
template<class T> class [[nodiscard]] Result {
    std::variant<T, Diagnostic> data_;
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(Diagnostic error) : data_(std::in_place_index<1>, std::move(error)) {}
    explicit operator bool() const noexcept { return data_.index() == 0; }
    T* get() noexcept { return std::get_if<0>(&data_); }
    const T* get() const noexcept { return std::get_if<0>(&data_); }
    const Diagnostic* error() const noexcept { return std::get_if<1>(&data_); }
};

// An owned value model mirroring MetaIL's scalar, signed-64 structured API.
struct Expr {
    metail_expr_kind kind = METAIL_INT;
    std::int64_t value = 0;
    std::string name; // variable, canonical operator, or callee
    std::vector<Expr> args; // ordered; select's arms are lazy
};
struct Statement {
    metail_stmt_kind kind = METAIL_RETURN;
    std::string name, target, yes, no;
    Expr expr;
};
struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<Statement> statements;
};

struct Pattern {
    enum class Kind { reg, immediate, operation } kind = Kind::reg;
    std::string op;
    std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    std::vector<Pattern> args;
    static Pattern reg();
    static Pattern imm(std::int64_t lo = std::numeric_limits<std::int64_t>::min(),
                       std::int64_t hi = std::numeric_limits<std::int64_t>::max());
    static Pattern operation(std::string op, std::vector<Pattern> args);
};
struct Rule {
    std::string name;
    Pattern pattern;
    std::string opcode;
    std::uint32_t cost = 1, latency = 1;
    std::string resource = "alu";
};
struct Backend {
    std::string name = "portable";
    std::uint32_t issue_width = 1;
    std::map<std::string, std::uint32_t, std::less<>> resources{{"alu", 1}, {"control", 1}};
    std::vector<Rule> rules;
    static Backend portable();
};
struct Module {
    std::string name;
    std::vector<Function> functions;
    Backend backend = Backend::portable();
};

struct PreprocessOptions {
    std::filesystem::path executable, standard_library; // empty: discover defaults
    std::vector<std::filesystem::path> include_directories;
    std::map<std::string, std::string, std::less<>> defines;
    std::size_t max_output_bytes = 8 * 1024 * 1024;
    unsigned timeout_seconds = 10;
};
struct ExpandedSource {
    std::string text;
    std::vector<std::filesystem::path> dependencies; // root + transitive includes + prelude
};
// Each expansion runs real GPPX in a separate process, without a shell or #exec.
// File access is NOT sandboxed. Only use trusted macro sources.
Result<ExpandedSource> preprocess(const std::filesystem::path&, const PreprocessOptions& = {});

class Compiler {
    struct ContextDeleter { void operator()(metail_context*) const noexcept; };
    std::unique_ptr<metail_context, ContextDeleter> context_;
public:
    // Initialization failures throw std::runtime_error. Other semantic failures
    // use Result. Allocation/filesystem exceptions are not translated.
    explicit Compiler(const char* metail_home = nullptr);
    Compiler(Compiler&&) noexcept = default;
    Compiler& operator=(Compiler&&) noexcept = default;
    // parse consumes expanded IR; load performs GPPX expansion first.
    Result<Module> parse(std::string_view, const std::string& file = "<juniper>");
    Result<Module> load(const std::filesystem::path&, const PreprocessOptions& = {});
    Result<bool> check(const Module&);
    Result<std::string> emit(const Module&); // complete, macro-free Juniper IR
    Result<std::string> emit_metail(const Module&);
    Result<Module> optimize(const Module&, const std::string& passes = "all", int rounds = 2);
    Result<std::string> analyze(const Module&, const std::string& passes = "all");
};

struct Operand {
    enum class Kind { reg, immediate, label, symbol } kind = Kind::reg;
    std::string name;
    std::int64_t value = 0;
};
struct Instruction {
    std::size_t id = 0, statement = 0;
    std::string opcode, result; // empty result means no definition
    std::vector<Operand> operands;
    std::uint32_t latency = 1;
    std::string resource = "alu";
    bool barrier = false;
    std::vector<std::size_t> dependencies; // original IDs, filled by schedule
    std::optional<std::uint64_t> cycle;
};
struct MachineFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instruction> instructions;
};
struct MachineModule {
    std::string name;
    Backend backend;
    std::vector<MachineFunction> functions;
    bool scheduled = false;
};
// Deterministic minimum-cost tree covering. No commutation, duplication, CSE,
// implicit legalization, physical register allocation, or ABI lowering.
// metail_home has the same lookup/ownership contract as Compiler's constructor;
// selection validates native-built modules through the canonical MetaIL API.
Result<MachineModule> select(const Module&, const char* metail_home = nullptr);
// RAW/WAR/WAW + full effect/control/trap fences; latency/issue/resource-aware
// critical-path list scheduling. Input is not modified, IDs remain stable.
Result<MachineModule> schedule(const MachineModule&);
std::string emit_machine_json(const MachineModule&);
std::string format(const Diagnostic&);

} // namespace juniper
