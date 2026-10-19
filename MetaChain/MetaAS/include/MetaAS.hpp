#ifndef METAAS_HPP
#define METAAS_HPP
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metaas {
struct Diagnostic {
    std::string file, message;
    std::size_t line=0, column=0;
};
template<class T> struct Result {
    std::optional<T> value;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const noexcept { return value.has_value() && diagnostics.empty(); }
};
using Bindings=std::map<std::string,std::vector<std::string>,std::less<>>;
struct Options {
    std::size_t target=0;
    std::uint64_t origin=0;
    std::size_t max_source_bytes=16u*1024u*1024u;
    std::size_t max_output_bytes=16u*1024u*1024u;
    std::size_t max_statements=100000, max_symbols=100000, max_sections=256;
    std::size_t max_expression_depth=64;
    Bindings bindings;
};
struct Section {
    std::string name;
    std::uint64_t address=0;
    std::size_t alignment=1;
    std::vector<std::uint8_t> bytes;
};
struct Symbol {
    std::string name;
    std::optional<std::size_t> section;
    std::uint64_t offset=0;
    bool global=false;
};
struct Relocation {
    std::size_t section=0, offset=0, size=0;
    std::string type, symbol, parameter, expression, addend_mode, overflow;
    std::int64_t addend=0;
    std::uint32_t bit_offset=0, bit_width=0;
    bool big_endian=false, pc_relative=false, is_signed=false;
    // Only assembler-generated data relocations may be resolved by binary().
    bool data=false;
};
struct Image {
    std::string architecture, subarchitecture, object_format, abi;
    std::uint64_t origin=0;
    std::size_t target=0;
    std::int64_t address_width=0;
    bool big_endian=false;
    std::vector<Section> sections;
    std::vector<Symbol> symbols;
    std::vector<Relocation> relocations;
    // Resolves absolute data references to defined symbols. Architecture-specific
    // relocations require a linker and are rejected, including local ones.
    Result<std::vector<std::uint8_t>> binary(std::size_t max_bytes=16u*1024u*1024u) const;
    // Deterministic, versioned MetaAS JSON object, not an ELF/COFF/Mach-O file.
    std::string json() const;
};
struct Encoder {
    std::string name;
    std::vector<std::string> parameters;
    std::size_t size=0;
};
class Assembler {
    struct Impl;
    std::shared_ptr<const Impl> impl_;
    explicit Assembler(std::shared_ptr<const Impl> impl):impl_(std::move(impl)) {}
public:
    // Supports MetaArch YAML/JSON/XML/S-expression specs and .march DSL files.
    // Loaded assemblers may be copied and used concurrently with distinct results.
    // Parse/assembly errors are diagnostics; allocation failures may throw.
    static Result<Assembler> load(const std::string& path);
    Result<Image> assemble(std::string_view source,const Options& options={},
                           const std::string& file="<assembly>") const;
    std::vector<Encoder> encoders() const;
    std::size_t target_count() const;
};
} // namespace metaas
#endif
