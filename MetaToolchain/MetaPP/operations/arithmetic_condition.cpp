#include "MetaPP-Internal.hpp"
#include <cctype>
#include <limits>
namespace metapp {
namespace {
struct Value { uint64_t bits = 0; bool uns = false; };
// Avoid implementation-defined unsigned-to-signed conversions.
int64_t signed_value(uint64_t v) {
    return v <= uint64_t(INT64_MAX) ? static_cast<int64_t>(v) : -1 - static_cast<int64_t>(~v);
}
class Expression {
    const Tokens &ts;
    size_t pos = 0;
    unsigned nesting = 0;
    Token location;
    bool take(const std::string &s) {
        if (pos < ts.size() && ts[pos].text == s) { ++pos; return true; }
        return false;
    }
    void expect(const std::string &s) { if (!take(s)) fail(location, "expected '" + s + "' in #if"); }
    static int precedence(const std::string &op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "|") return 3;
        if (op == "^") return 4;
        if (op == "&") return 5;
        if (op == "==" || op == "!=") return 6;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
        if (op == "<<" || op == ">>") return 8;
        if (op == "+" || op == "-") return 9;
        if (op == "*" || op == "/" || op == "%") return 10;
        return 0;
    }
    Value literal(const Token &t) {
        if (t.kind == Kind::identifier) return {};
        if (t.kind == Kind::character) {
            auto begin = t.text.find('\'') + 1;
            size_t end = t.text.size() - 1;
            uint64_t value = 0;
            if (begin == end) fail(t, "empty character constant");
            for (size_t i = begin; i < end; ++i) {
                unsigned c = static_cast<unsigned char>(t.text[i]);
                if (c == '\\') {
                    c = static_cast<unsigned char>(t.text[++i]);
                    const std::string escapes = "abfnrtv\\'\"?";
                    const std::string values = "\a\b\f\n\r\t\v\\'\"?";
                    auto p = escapes.find(static_cast<char>(c));
                    if (p != std::string::npos) c = static_cast<unsigned char>(values[p]);
                    else if (c >= '0' && c <= '7') {
                        c -= '0';
                        for (int n = 1; n < 3 && i + 1 < end && t.text[i + 1] >= '0' && t.text[i + 1] <= '7'; ++n) c = c * 8 + unsigned(t.text[++i] - '0');
                    } else if (c == 'x' || c == 'u' || c == 'U') {
                        unsigned required = c == 'u' ? 4 : (c == 'U' ? 8 : 0), count = 0;
                        c = 0;
                        while (i + 1 < end && std::isxdigit(static_cast<unsigned char>(t.text[i + 1])) && (!required || count < required)) {
                            char d = t.text[++i]; c = c * 16 + unsigned(std::isdigit(static_cast<unsigned char>(d)) ? d - '0' : std::tolower(static_cast<unsigned char>(d)) - 'a' + 10); ++count;
                        }
                        if (!count || (required && count != required)) fail(t, "invalid character escape");
                    } else fail(t, "unknown character escape");
                }
                value = (value << 8) | c;
            }
            return {value, false};
        }
        if (t.kind != Kind::number) fail(t, "expected integer expression");
        const std::string &s = t.text;
        size_t i = 0;
        unsigned base = 10;
        if (s.size() > 1 && s[0] == '0') {
            base = 8;
            if (s[1] == 'x' || s[1] == 'X') { base = 16; i = 2; }
        }
        size_t begin = i;
        uint64_t value = 0;
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            unsigned d = std::isdigit(c) ? unsigned(c - '0') : (std::isxdigit(c) ? unsigned(std::tolower(c) - 'a' + 10) : 99);
            if (d >= base) break;
            if (value > (UINT64_MAX - d) / base) fail(t, "integer constant out of range");
            value = value * base + d; ++i;
        }
        if (i == begin) fail(t, "invalid integer constant");
        std::string suffix = s.substr(i);
        if (suffix.find("lL") != std::string::npos || suffix.find("Ll") != std::string::npos) fail(t, "invalid integer suffix");
        for (char &c : suffix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (suffix != "" && suffix != "u" && suffix != "l" && suffix != "ul" && suffix != "lu" && suffix != "ll" && suffix != "ull" && suffix != "llu") fail(t, "invalid integer suffix");
        bool uns = suffix.find('u') != std::string::npos || (base != 10 && value > uint64_t(INT64_MAX));
        if (!uns && value > uint64_t(INT64_MAX)) fail(t, "signed integer constant out of range");
        return {value, uns};
    }
    Value unary(bool live) {
        if (++nesting > 256) fail(location, "expression nesting exceeds 256");
        Value v;
        if (take("!")) { v = unary(live); v = {uint64_t(!v.bits), false}; }
        else if (take("~")) { v = unary(live); v.bits = ~v.bits; }
        else if (take("+")) v = unary(live);
        else if (take("-")) { v = unary(live); v.bits = 0 - v.bits; }
        else if (take("(")) { v = conditional(live); expect(")"); }
        else {
            if (pos == ts.size()) fail(location, "missing operand in #if");
            v = literal(ts[pos++]);
        }
        --nesting; return v;
    }
    Value binary(int minimum, bool live) {
        Value a = unary(live);
        while (pos < ts.size()) {
            Token op = ts[pos]; int p = precedence(op.text);
            if (p < minimum) break;
            ++pos;
            bool right_live = live && !(op.text == "&&" && !a.bits) && !(op.text == "||" && a.bits);
            Value b = binary(p + 1, right_live);
            bool uns = a.uns || b.uns;
            uint64_t x = a.bits, y = b.bits;
            int64_t sx = signed_value(x), sy = signed_value(y);
            const std::string &s = op.text;
            Value r{0, uns};
            if (s == "&&") r = {uint64_t(x && y), false};
            else if (s == "||") r = {uint64_t(x || y), false};
            else if (s == "==") r = {uint64_t(x == y), false};
            else if (s == "!=") r = {uint64_t(x != y), false};
            else if (s == "<") r = {uint64_t(uns ? x < y : sx < sy), false};
            else if (s == ">") r = {uint64_t(uns ? x > y : sx > sy), false};
            else if (s == "<=") r = {uint64_t(uns ? x <= y : sx <= sy), false};
            else if (s == ">=") r = {uint64_t(uns ? x >= y : sx >= sy), false};
            else if (s == "+") r.bits = x + y;
            else if (s == "-") r.bits = x - y;
            else if (s == "*") r.bits = x * y;
            else if (s == "&") r.bits = x & y;
            else if (s == "|") r.bits = x | y;
            else if (s == "^") r.bits = x ^ y;
            else if (s == "/" || s == "%") {
                if (live && !y) fail(op, "division by zero in #if");
                if (live && !uns && sx == INT64_MIN && sy == -1) fail(op, "signed division overflow in #if");
                if (y && (uns || sx != INT64_MIN || sy != -1)) r.bits = uns ? (s == "/" ? x / y : x % y) : uint64_t(s == "/" ? sx / sy : sx % sy);
            } else if (s == "<<" || s == ">>") {
                r.uns = a.uns;
                if (live && y >= 64) fail(op, "shift count outside [0,63]");
                if (y < 64) {
                    if (s == "<<") r.bits = x << y;
                    else { r.bits = x >> y; if (!a.uns && sx < 0 && y) r.bits |= UINT64_MAX << (64 - y); }
                }
            }
            a = r;
        }
        return a;
    }
    Value conditional(bool live) {
        if (++nesting > 256) fail(location, "expression nesting exceeds 256");
        Value c = binary(1, live);
        if (take("?")) {
            Value a = conditional(live && c.bits); expect(":");
            Value b = conditional(live && !c.bits);
            c = {c.bits ? a.bits : b.bits, a.uns || b.uns};
        }
        --nesting; return c;
    }
public:
    explicit Expression(const Tokens &tokens) : ts(tokens) {
        if (!ts.empty()) location = ts.front();
    }
    bool run() {
        auto v = conditional(true);
        if (pos != ts.size()) fail(ts[pos], "unexpected token in #if: " + ts[pos].text);
        return v.bits != 0;
    }
};
}
bool Engine::evaluate(Tokens ts) {
    Tokens replaced;
    for (size_t i = 0; i < ts.size(); ++i) {
        Token t = ts[i];
        if (t.text == "defined") {
            bool paren = i + 1 < ts.size() && ts[i + 1].text == "(";
            if (paren) ++i;
            if (++i == ts.size() || ts[i].kind != Kind::identifier) fail(t, "defined requires an identifier");
            bool exists = macros.count(ts[i].text) || builtin(ts[i].text);
            if (paren && (++i == ts.size() || ts[i].text != ")")) fail(t, "expected ')' after defined");
            t.kind = Kind::number; t.text = exists ? "1" : "0";
        }
        replaced.push_back(t);
    }
    auto expanded = expand(std::move(replaced));
    return Expression(expanded).run();
}
}
