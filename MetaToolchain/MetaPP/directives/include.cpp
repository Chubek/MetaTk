#include "MetaPP-Internal.hpp"
#include <filesystem>
namespace metapp {
std::string Engine::include(const Tokens &input, const std::string &file) {
    if (input.empty()) throw Error(file + ": missing include filename");
    Tokens ts = input;
    if (ts[0].kind != Kind::string && ts[0].kind != Kind::header && ts[0].text != "<") ts = expand(ts);
    std::string name;
    bool quoted = false;
    if (ts.size() == 1 && ts[0].kind == Kind::string && ts[0].text[0] == '"') {
        name = ts[0].text.substr(1, ts[0].text.size() - 2); quoted = true;
    } else if (ts.size() == 1 && ts[0].kind == Kind::header) {
        name = ts[0].text.substr(1, ts[0].text.size() - 2);
    } else if (ts.size() >= 3 && ts.front().text == "<" && ts.back().text == ">") {
        for (size_t i = 1; i + 1 < ts.size(); ++i) {
            if (i > 1 && ts[i].space) name += ' ';
            name += ts[i].text;
        }
    } else fail(input[0], "expected quoted or angle-bracket include filename");
    if (name.empty()) fail(input[0], "empty include filename");
    std::vector<std::filesystem::path> directories;
    if (quoted) directories.push_back(std::filesystem::path(file).parent_path());
    for (const auto &p : paths) directories.emplace_back(p);
    if (std::filesystem::path(name).is_absolute()) directories.insert(directories.begin(), "");
    for (const auto &dir : directories) {
        auto path = dir / name;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) continue;
        auto canonical = std::filesystem::weakly_canonical(path).string();
        if (once.count(canonical)) return {};
        return process(read_file(path.string()), path.string());
    }
    fail(input[0], "include file not found: " + name);
}
}
