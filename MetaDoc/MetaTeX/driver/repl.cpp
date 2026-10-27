#include "Driver.hpp"
namespace metatex::driver {
bool incompleteSource(const std::string& source) {
    int groups = 0, environments = 0, conditionals = 0; std::string math;
    auto tokens = Lexer::scan(source);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.kind == TokenKind::BeginGroup) ++groups;
        if (token.kind == TokenKind::EndGroup) --groups;
        if (token.kind == TokenKind::MathShift) {
            std::string delimiter = "$";
            if (i + 1 < tokens.size() && tokens[i + 1].kind == TokenKind::MathShift) { delimiter = "$$"; ++i; }
            if (math.empty()) math = delimiter; else if (math == delimiter) math.clear();
        }
        if (token.kind == TokenKind::Control) {
            if (token.value == "begin") ++environments;
            if (token.value == "end") --environments;
            if (token.value == "iftrue" || token.value == "iffalse" || token.value == "ifdefined" || token.value == "ifnum" || token.value == "ifdim") ++conditionals;
            if (token.value == "fi") --conditionals;
            if (token.value == "(" || token.value == "[") math = token.value;
            if (token.value == ")" || token.value == "]") math.clear();
        }
    }
    return groups > 0 || environments > 0 || conditionals > 0 || !math.empty();
}
}
