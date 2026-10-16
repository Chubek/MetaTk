// C API for QScript-Grammar.h.
#include "qscript/include/QScript-Grammar.h"
#include "qscript/src/Context.h"
#include "qscript/src/Internal.h"
#include <cstring>

using qscript::detail::protect;
using qscript::detail::report;
using qscript::detail::text_out;

extern "C" {
qs_grammar* qs_grammar_compile(qs_context* c, const char* source, size_t length) {
    qs_grammar* g = nullptr;
    protect(c, [&] {
        if (!source && length) { c->error = "null grammar source"; return QS_INVALID_ARGUMENT; }
        auto compiled = qs_grammar::compile(std::string_view(source ? source : "", length));
        if (!compiled.ok()) { report(c, {{metachain::Stage::Parse, compiled.error, {}, 0, 0, 0}}); return QS_ERROR; }
        g = new qs_grammar(std::move(compiled));
        return QS_OK;
    });
    return g;
}
const char* qs_grammar_qscript_source(void) { return qscript::detail::embedded_grammar(); }
qs_grammar* qs_grammar_qscript(qs_context* c) {
    qs_grammar* g = nullptr;
    protect(c, [&] { g = new qs_grammar(qscript::language_grammar()); return QS_OK; });
    return g;
}
void qs_grammar_destroy(qs_grammar* g) { delete g; }
const char* qs_grammar_name(const qs_grammar* g) { return g ? g->name.c_str() : nullptr; }
size_t qs_grammar_rule_count(const qs_grammar* g) {
    if (!g) return 0;
    try { return g->rule_names().size(); } catch (...) { return 0; }
}
const char* qs_grammar_rule_name(const qs_grammar* g, size_t index) {
    if (!g) return nullptr;
    // Declarations are kept in order; return the index-th distinct name.
    std::size_t seen = 0;
    for (std::size_t i = 0; i < g->declarations.size(); ++i) {
        bool first = true;
        for (std::size_t j = 0; j < i; ++j) if (g->declarations[j].name == g->declarations[i].name) { first = false; break; }
        if (first && seen++ == index) return g->declarations[i].name.c_str();
    }
    return nullptr;
}
qs_status qs_grammar_peg(qs_context* c, const qs_grammar* g, const char** text, size_t* length) {
    if (text) *text = nullptr;
    if (length) *length = 0;
    return protect(c, [&] {
        if (!g || !text || !length) { c->error = "null grammar argument"; return QS_INVALID_ARGUMENT; }
        return text_out(c, g->peg(), text, length);
    });
}
qs_match* qs_grammar_parse(qs_context* c, const qs_grammar* g, const char* input, size_t length, const char* start) {
    qs_match* m = nullptr;
    protect(c, [&] {
        if (!g || (!input && length)) { c->error = "null parse argument"; return QS_INVALID_ARGUMENT; }
        auto r = g->parse(std::string_view(input ? input : "", length), start ? start : "TOP");
        if (!r) { report(c, {{metachain::Stage::Parse, r.error, {}, r.line, r.column, r.offset}}); return QS_ERROR; }
        m = new qs_match(std::move(*r.match));
        return QS_OK;
    });
    return m;
}
void qs_match_destroy(qs_match* m) { delete m; }
const char* qs_match_name(const qs_match* m) { return m ? m->name.c_str() : nullptr; }
const char* qs_match_sym(const qs_match* m) { return m ? m->sym.c_str() : nullptr; }
const char* qs_match_text(const qs_match* m, size_t* length) {
    if (length) *length = m ? m->text.size() : 0;
    return m ? m->text.c_str() : nullptr;
}
qs_status qs_match_span(const qs_match* m, qs_span* s) {
    if (!m || !s) return QS_INVALID_ARGUMENT;
    *s = qs_span{m->from, m->to, m->line, m->column};
    return QS_OK;
}
size_t qs_match_count(const qs_match* m) { return m ? m->children.size() : 0; }
const qs_match* qs_match_child(const qs_match* m, size_t i) { return m && i < m->children.size() ? &m->children[i] : nullptr; }
const qs_match* qs_match_find(const qs_match* m, const char* name) { return m && name ? m->find(name) : nullptr; }
qs_status qs_match_walk(const qs_match* m, qs_match_visitor fn, void* user) {
    if (!m || !fn) return QS_INVALID_ARGUMENT;
    std::vector<std::pair<const qs_match*, std::size_t>> work{{m, 0}};
    while (!work.empty()) {
        auto [node, depth] = work.back(); work.pop_back();
        if (!fn(node, depth, user)) continue;
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) work.emplace_back(&*it, depth + 1);
    }
    return QS_OK;
}
qs_status qs_match_dump(qs_context* c, const qs_match* m, const char** text, size_t* length) {
    if (text) *text = nullptr;
    if (length) *length = 0;
    return protect(c, [&] {
        if (!m || !text || !length) { c->error = "null match argument"; return QS_INVALID_ARGUMENT; }
        return text_out(c, m->dump(), text, length);
    });
}
} // extern C
