#pragma once
#include "doxtk/DoxTk.hpp"
#include <functional>
#include <map>

// IR-to-IR translation interfaces.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkTranslator.cpp.
namespace doxtk {
using Translation = std::function<Result<Document>(const Document&, const ConvertOptions&)>;

// Instance-owned direct routes. Register before sharing with concurrent readers;
// callbacks must themselves be safe for concurrent use. No implicit chaining.
class ConversionRegistry final : public Translator {
public:
    [[nodiscard]] Error add(std::string source_ir, std::string target_ir, Translation translate);
    [[nodiscard]] bool supports(std::string_view source_ir, std::string_view target_ir) const;
    [[nodiscard]] Result<Document> translate(const Document& input, std::string_view target_ir,
        const ConvertOptions& options = {}) const override;
private:
    std::map<std::pair<std::string, std::string>, Translation> routes_;
};

[[nodiscard]] ConversionRegistry builtin_conversions();

struct TranslatorModuleInfo {
    static constexpr std::string_view description = "IR-to-IR translation interfaces.";
};
} // namespace doxtk
