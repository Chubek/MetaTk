#include "MetaPP-Internal.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <locale>
namespace metapp {
Engine::Engine() {
    // localtime's shared storage is protected across independent contexts.
    static std::mutex clock_mutex;
    std::lock_guard<std::mutex> lock(clock_mutex);
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto tm = std::localtime(&now);
    if (!tm) { date = "??? ?? ????"; time = "??:??:??"; return; }
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::ostringstream d, t;
    d.imbue(std::locale::classic()); t.imbue(std::locale::classic());
    d << months[tm->tm_mon] << ' ' << std::setw(2) << tm->tm_mday << ' ' << tm->tm_year + 1900;
    t << std::setfill('0') << std::setw(2) << tm->tm_hour << ':' << std::setw(2) << tm->tm_min << ':' << std::setw(2) << tm->tm_sec;
    date = d.str(); time = t.str();
}
bool Engine::builtin(const std::string &name) const {
    return name == "__FILE__" || name == "__LINE__" || name == "__DATE__" || name == "__TIME__" || name == "__STDC__" || name == "__STDC_HOSTED__" || name == "__STDC_VERSION__";
}
void Engine::warning(const Token &at, const std::string &text) {
    diagnostics += at.file + ":" + std::to_string(at.line) + ": warning: " + text + '\n';
}
std::string Engine::process(const std::string &source, const std::string &file) {
    if (include_depth >= 128) throw Error(file + ": include nesting exceeds 128");
    if (++files_processed > max_files) throw Error(file + ": included file count limit exceeded");
    if (source.size() > 64 * 1024 * 1024) throw Error(file + ": input exceeds 64 MiB limit");
    struct Guard {
        Engine &e;
        Guard(Engine &engine, const std::string &path) : e(engine) { e.files.push_back(path); ++e.include_depth; }
        ~Guard() { --e.include_depth; e.files.pop_back(); }
    } guard(*this, file);
    auto ts = lex(source, file);
    struct Conditional { bool parent, taken, active, saw_else; Token start; };
    std::vector<Conditional> conditions;
    auto active = [&]() { return conditions.empty() || conditions.back().active; };
    std::string output, logical_file = file;
    int64_t line_delta = 0;
    Tokens pending;
    auto flush = [&]() { append_text(output, render(expand(std::move(pending)))); pending.clear(); };
    for (size_t i = 0; i < ts.size();) {
        size_t end = i;
        while (end < ts.size() && ts[end].kind != Kind::newline) ++end;
        unsigned next_line = end < ts.size() ? ts[end].line + 1 : (end > i ? ts[end - 1].line + 1 : 1);
        Tokens line(ts.begin() + static_cast<ptrdiff_t>(i), ts.begin() + static_cast<ptrdiff_t>(end));
        for (auto &t : line) { t.file = logical_file; t.line = static_cast<unsigned>(int64_t(t.line) + line_delta); }
        bool directive = !line.empty() && line[0].is("#");
        if (directive) {
            flush();
            if (line.size() > 1) {
                Token at = line[1]; const auto &cmd = at.text;
                Tokens rest(line.begin() + 2, line.end());
                if (cmd == "if" || cmd == "ifdef" || cmd == "ifndef") {
                    if (conditions.size() >= 256) fail(at, "conditional nesting exceeds 256");
                    bool parent = active(), value = false;
                    if (cmd == "if") {
                        if (parent) {
                            if (rest.empty()) fail(at, "missing expression in #if");
                            value = evaluate(rest);
                        }
                    }
                    else {
                        if (rest.size() != 1 || rest[0].kind != Kind::identifier) fail(at, "expected one macro name");
                        value = macros.count(rest[0].text) || builtin(rest[0].text);
                        if (cmd == "ifndef") value = !value;
                    }
                    conditions.push_back({parent, parent && value, parent && value, false, at});
                } else if (cmd == "elif" || cmd == "else" || cmd == "endif") {
                    if (conditions.empty()) fail(at, "unmatched #" + cmd);
                    auto &c = conditions.back();
                    if (cmd == "endif") {
                        if (!rest.empty()) fail(at, "extra tokens after #endif");
                        conditions.pop_back();
                    } else {
                        if (c.saw_else) fail(at, "#" + cmd + " after #else");
                        if (cmd == "else" && !rest.empty()) fail(at, "extra tokens after #else");
                        bool value = c.parent && !c.taken;
                        if (cmd == "elif" && value) {
                            if (rest.empty()) fail(at, "missing expression in #elif");
                            value = evaluate(rest);
                        }
                        c.active = value; c.taken = c.taken || value;
                        if (cmd == "else") c.saw_else = true;
                    }
                } else if (active()) {
                    if (cmd == "define") {
                        if (rest.empty()) fail(at, "#define requires a macro name");
                        define(rest);
                    }
                    else if (cmd == "undef") {
                        if (rest.size() != 1 || rest[0].kind != Kind::identifier) fail(at, "expected one macro name");
                        if (builtin(rest[0].text)) fail(at, "cannot undefine reserved macro");
                        macros.erase(rest[0].text);
                    } else if (cmd == "include") {
                        if (rest.empty()) fail(at, "missing include filename");
                        append_text(output, include(rest, file));
                    }
                    else if (cmd == "error") fail(at, render(rest));
                    else if (cmd == "warning") warning(at, render(rest));
                    else if (cmd == "pragma") {
                        if (rest.size() == 1 && rest[0].text == "once") once.insert(std::filesystem::weakly_canonical(file).string());
                        else append_text(output, "#pragma " + render(rest) + '\n');
                    } else if (cmd == "line") {
                        rest = expand(rest);
                        if (rest.empty() || rest.size() > 2 || rest[0].kind != Kind::number) fail(at, "invalid #line");
                        uint64_t number = 0;
                        for (char ch : rest[0].text) {
                            if (ch < '0' || ch > '9' || number > 214748364U) fail(at, "invalid #line number");
                            number = number * 10 + unsigned(ch - '0');
                        }
                        if (!number || number > 2147483647U) fail(at, "#line number out of range");
                        if (rest.size() == 2) {
                            if (rest[1].kind != Kind::string || rest[1].text[0] != '"') fail(at, "expected #line filename");
                            logical_file.clear();
                            const auto &s = rest[1].text;
                            for (size_t p = 1; p + 1 < s.size(); ++p) {
                                if (s[p] == '\\' && p + 2 < s.size() && (s[p + 1] == '\\' || s[p + 1] == '"')) ++p;
                                logical_file += s[p];
                            }
                        }
                        line_delta = int64_t(number) - next_line;
                    } else fail(at, "unknown preprocessing directive #" + cmd);
                }
            }
            output += '\n';
        } else if (active()) {
            pending.insert(pending.end(), line.begin(), line.end());
            if (end < ts.size()) pending.push_back(ts[end]);
        } else output += '\n';
        i = end < ts.size() ? end + 1 : end;
        if (output.size() > 64 * 1024 * 1024) throw Error(file + ": output exceeds 64 MiB limit");
    }
    if (!conditions.empty()) fail(conditions.back().start, "unterminated conditional directive");
    flush();
    if (!output.empty() && output.back() != '\n') output += '\n';
    if (output.size() > 64 * 1024 * 1024) throw Error(file + ": output exceeds 64 MiB limit");
    return output;
}
}
