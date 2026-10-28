#ifndef METAVM_VM_HPP
#define METAVM_VM_HPP
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace metavm {
struct Frame;
// Numeric values are the version-1 wire ABI: append, never reorder.
enum class Op : std::uint32_t {
    Constant, Move, Add, Sub, Mul, Div, Mod, Shl, Shr, Band, Bor, Bxor,
    Neg, Bnot, Lnot, Eq, Ne, Lt, Le, Gt, Ge, Land, Lor,
    Jump, Branch, Call, Return
};
struct Instruction {
    Op op;
    std::uint32_t dst = 0, a = 0, b = 0;
    std::int64_t imm = 0;
};
struct Limits {
    std::uint32_t slots = 4096, instructions = 1000000, hosts = 1024;
    std::size_t input_bytes = 32 * 1024 * 1024;
};
/** Immutable validated single-function register bytecode. Owns all storage;
 * concurrent const reads are safe. Construction/deserialization throw
 * invalid_argument/length_error/bad_alloc; no partial program is returned. */
class Program {
public:
    static constexpr std::uint32_t version = 1;
    Program(std::uint32_t slots, std::uint32_t parameters,
            std::vector<Instruction> code, std::vector<std::string> hosts = {}, Limits limits = {});
    std::uint32_t slots() const noexcept { return slots_; }
    std::uint32_t parameters() const noexcept { return parameters_; }
    const std::vector<Instruction> &code() const noexcept { return code_; }
    const std::vector<std::string> &hosts() const noexcept { return hosts_; }
    std::vector<std::uint8_t> serialize() const;
    static Program deserialize(const std::vector<std::uint8_t> &bytes, Limits limits = {});
private:
    friend Frame make_frame(const Program &, const std::vector<std::int64_t> &);
    friend void validate_frame(const Program &, const Frame &);
    std::shared_ptr<const void> identity_ = std::make_shared<const char>(0);
    std::uint32_t slots_, parameters_;
    std::vector<Instruction> code_;
    std::vector<std::string> hosts_;
};
enum class Status : std::uint64_t { Running, Returned, Trapped };
enum class Trap : std::uint64_t { None, DivisionByZero, DivisionOverflow, InvalidShift, MissingHost, HostFailure };
enum class Stop { Budget, Returned, Trapped };
struct Frame {
    // Opaque program identity: Program copies share it; deserialization creates
    // a new identity. Do not modify this field or active while executing.
    std::shared_ptr<const void> program_identity;
    bool active = false;
    std::vector<std::int64_t> values;
    std::uint64_t pc = 0, steps = 0;
    Status status = Status::Running;
    std::int64_t result = 0;
    Trap trap = Trap::None;
    std::string diagnostic;
};
using Host = std::function<std::int64_t(std::string_view, const std::vector<std::int64_t> &)>;
/** Create a zero-initialized frame, with arguments in slots [0,parameters).
 * Wrong arity throws invalid_argument. Frames are caller-owned and mutable. */
Frame make_frame(const Program &, const std::vector<std::int64_t> &arguments);
/** Validate externally supplied/resumed state. Throws invalid_argument; callers
 * must synchronize mutations. A frame must belong to the same Program. */
void validate_frame(const Program &, const Frame &);
/** Execute at most budget instructions; budget exhaustion leaves Running state
 * at the next instruction. Traps retain the faulting PC and consume one step.
 * Host exceptions become HostFailure, except bad_alloc which propagates after
 * marking the frame trapped (never replay a partially performed host call).
 * No process signal or signed C++ overflow is used to implement VM traps. */
Stop run(const Program &, Frame &, std::uint64_t budget, const Host &host = {});
} // namespace metavm
#endif
