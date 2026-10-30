#ifndef METAPP_INTERNAL_HPP
#define METAPP_INTERNAL_HPP
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace metapp {
inline constexpr size_t max_text_bytes = 64 * 1024 * 1024;
inline constexpr size_t max_tokens = 1000000;
inline constexpr size_t max_files = 4096;
enum class Kind { identifier, number, string, character, header, punct, newline, placemarker };
struct Token {
    Kind kind = Kind::punct;
    std::string text;
    bool space = false;
    unsigned line = 1;
    std::string file;
    std::set<std::string> hide;
    // Digraphs retain their spelling for #, ## and output, but act as the
    // equivalent punctuator when interpreted as a preprocessing operator.
    bool is(const std::string &op) const {
        return text == op || (op == "#" && text == "%:") ||
            (op == "##" && text == "%:%:");
    }
};
using Tokens = std::vector<Token>;
struct Macro {
    bool function = false, variadic = false;
    std::vector<std::string> params;
    Tokens body;
};
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
[[noreturn]] void fail(const Token &at, const std::string &message);
Tokens lex(const std::string &source, const std::string &file, bool source_phases = true);
void append_text(std::string &out, std::string_view text);
std::string render(const Tokens &tokens);
std::string quote(const std::string &text);
std::string read_file(const std::string &path);
bool identifier(const std::string &text);
class Engine {
public:
    std::map<std::string, Macro> macros;
    std::vector<std::string> paths;
    std::set<std::string> once;
    std::vector<std::string> files;
    std::string diagnostics, date, time;
    size_t steps = 0;
    size_t expansion_bytes = 0, files_processed = 0;
    unsigned include_depth = 0;
    Engine();
    void define(const Tokens &tokens);
    Tokens expand(Tokens tokens, unsigned depth = 0);
    bool evaluate(Tokens tokens);
    std::string process(const std::string &source, const std::string &file);
    std::string include(const Tokens &tokens, const std::string &file);
    void warning(const Token &at, const std::string &text);
    bool builtin(const std::string &name) const;
};
}
#endif
