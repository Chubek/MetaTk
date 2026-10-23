/* C++20 ownership and assembler API. The existing C API remains independent. */
#ifndef METAARCH_HPP
#define METAARCH_HPP

#include "ArchDB.h"
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace metaarch {

struct Diagnostic {
    DsymSeverity severity = DSYM_ERROR;
    std::string file, path, message;
    size_t line = 0, column = 0;
};
struct Error { std::vector<Diagnostic> diagnostics; };

// Checked access through get()/error(); no exception-based parse failures.
template<class T> class [[nodiscard]] Result {
    std::variant<T, Error> data_;
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : data_(std::in_place_index<1>, std::move(error)) {}
    explicit operator bool() const noexcept { return data_.index() == 0; }
    T *get() noexcept { return std::get_if<0>(&data_); }
    const T *get() const noexcept { return std::get_if<0>(&data_); }
    const Error *error() const noexcept { return std::get_if<1>(&data_); }
};

using Operand = std::variant<int64_t, std::string>; // integer or register name
struct Binding { std::string name; Operand value; };
// Explicit source order -> encoder parameter order, never guessed from bytecode.
using AssemblyBindings = std::map<std::string, std::vector<std::string>, std::less<>>;

struct Relocation {
    size_t offset, size;
    int32_t index;
    std::string name, parameter;
    int64_t value;
    size_t bit_offset = 0, bit_width = 0;
    DsymEndian endian = DSYM_ENDIAN_TARGET; // Actual emitted field order after encoding.
};
struct Encoded {
    std::vector<uint8_t> bytes;
    std::vector<Relocation> relocations;
};
struct ParseOptions {
    uint32_t load_flags = 0;
    size_t max_depth = 64;
    size_t max_source_bytes = 16 * 1024 * 1024;
};
struct AssembleOptions {
    size_t target = 0;
    size_t max_output_bytes = 16 * 1024 * 1024;
    AssemblyBindings bindings; // overrides architecture DSL assembly declarations
};

class Spec {
    std::unique_ptr<DsymSpec, decltype(&dsym_spec_free)> spec_;
    AssemblyBindings bindings_;
    std::vector<Diagnostic> diagnostics_;
    explicit Spec(DsymSpec *s) : spec_(s, dsym_spec_free) {}
public:
    Spec(Spec &&) noexcept = default;
    Spec &operator=(Spec &&) noexcept = default;
    Spec(const Spec &) = delete;
    Spec &operator=(const Spec &) = delete;

    static Result<Spec> load(const std::string &path, uint32_t flags = 0);
    static Result<Spec> parse(std::string_view source, DsymFormat format,
                              const std::string &path = "<memory>", uint32_t flags = 0);
    static Result<Spec> parse_dsl(std::string_view source,
                                  const std::string &path = "<dsl>", ParseOptions options = {});
    // .march files use the architecture DSL; all other extensions use C codecs.
    static Result<Spec> load_dsl(const std::string &path, ParseOptions options = {});
    const DsymSpec *get() const noexcept { return spec_.get(); }
    const AssemblyBindings &assembly_bindings() const noexcept { return bindings_; }
    const std::vector<Diagnostic> &diagnostics() const noexcept { return diagnostics_; }
    const DsymRegister *find_register(const std::string &name) const;
    Result<std::string> write(DsymFormat format, uint32_t flags = 0) const;
    Result<Encoded> encode(const std::string &encoder, std::span<const Binding> operands,
                            size_t target = 0) const;
    // parameters[i] explicitly maps form.operands[i] to an encoder input.
    // Checks kind, register class/width, immediate bounds and target features.
    Result<Encoded> encode_form(size_t form, std::span<const std::string> parameters,
                                 std::span<const Operand> operands, size_t target = 0) const;
    // Encoder-oriented assembly: named operands work without a binding table.
    // Positional syntax requires an explicit assembly binding. No labels,
    // directives, expressions, memory addressing, or implicit relocations.
    Result<Encoded> assemble(std::string_view source, const std::string &path = "<assembly>",
                              const AssembleOptions &options = {}) const;
};

} // namespace metaarch
#endif
