#include "MetaPP-Internal.hpp"
#include <algorithm>
namespace metapp {
void Engine::define(const Tokens &ts) {
    if (ts.empty()) throw Error("#define requires a macro name");
    if (ts[0].kind != Kind::identifier) fail(ts[0], "expected macro name");
    const std::string name = ts[0].text;
    if (builtin(name) || name == "defined" || name == "_Pragma") fail(ts[0], "cannot redefine reserved macro " + name);
    Macro m;
    size_t i = 1;
    if (i < ts.size() && ts[i].text == "(" && !ts[i].space) {
        m.function = true; ++i;
        if (i < ts.size() && ts[i].text != ")") {
            for (;;) {
                if (i == ts.size()) fail(ts[0], "unterminated macro parameter list");
                if (ts[i].text == "...") { m.variadic = true; m.params.push_back("__VA_ARGS__"); ++i; break; }
                if (ts[i].kind != Kind::identifier || ts[i].text == "__VA_ARGS__") fail(ts[i], "invalid parameter");
                if (std::find(m.params.begin(), m.params.end(), ts[i].text) != m.params.end()) fail(ts[i], "duplicate parameter");
                m.params.push_back(ts[i++].text);
                if (i == ts.size() || ts[i].text != ",") break;
                ++i;
            }
        }
        if (i == ts.size() || ts[i].text != ")") fail(ts[0], "expected ')' in macro definition");
        ++i;
    }
    m.body.assign(ts.begin() + static_cast<ptrdiff_t>(i), ts.end());
    for (size_t j = 0; j < m.body.size(); ++j) {
        const auto &t = m.body[j];
        if (t.is("##") && (j == 0 || j + 1 == m.body.size())) fail(t, "## cannot appear at either end of a replacement list");
        if (t.is("##") && j > 0 && m.body[j - 1].is("##")) fail(t, "consecutive ## operators");
        if (t.is("#") && m.function && (j + 1 == m.body.size() ||
            std::find(m.params.begin(), m.params.end(), m.body[j + 1].text) == m.params.end())) fail(t, "# must precede a parameter");
        if (t.text == "__VA_ARGS__" && !m.variadic) fail(t, "__VA_ARGS__ requires a variadic macro");
    }
    auto old = macros.find(name);
    if (old != macros.end()) {
        const auto &o = old->second;
        bool same = o.function == m.function && o.variadic == m.variadic && o.params == m.params && o.body.size() == m.body.size();
        for (size_t j = 0; same && j < m.body.size(); ++j)
            same = o.body[j].text == m.body[j].text && (j == 0 || o.body[j].space == m.body[j].space);
        if (!same) fail(ts[0], "incompatible redefinition of " + name);
    }
    macros[name] = std::move(m);
}
}
