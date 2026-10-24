#include "Internal.hpp"
#include <array>

namespace archas::detail {
std::pair<size_t, int64_t> character(std::string_view text, size_t start) {
    size_t pos = start + 1;
    if (pos >= text.size()) throw Error("incomplete character constant");
    unsigned value = static_cast<unsigned char>(text[pos++]);
    if (value == '\\') {
        if (pos >= text.size()) throw Error("incomplete character escape");
        char c = text[pos++];
        switch (c) {
        case 'a': value = 7; break;
        case 'b': value = 8; break;
        case 'f': value = 12; break;
        case 'n': value = 10; break;
        case 'r': value = 13; break;
        case 't': value = 9; break;
        case 'v': value = 11; break;
        default:
            if (c >= '0' && c <= '7') {
                value = unsigned(c - '0');
                for (int i = 1; i < 3 && pos < text.size() && text[pos] >= '0' && text[pos] <= '7'; ++i)
                    value = value * 8 + unsigned(text[pos++] - '0');
            } else value = static_cast<unsigned char>(c);
        }
    }
    if (value > 255) throw Error("character constant exceeds one byte");
    if (pos < text.size() && text[pos] == '\'') ++pos;
    return {pos, int64_t(value)};
}
namespace {
int64_t add(int64_t a, int64_t b) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        throw Error("integer expression overflow");
    return a + b;
}
int64_t negate(int64_t a) {
    if (a == INT64_MIN) throw Error("integer expression overflow");
    return -a;
}
int64_t multiply(int64_t a, int64_t b) {
    if ((a > 0 && ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a))) ||
        (a < 0 && ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b))))
        throw Error("integer expression overflow");
    return a * b;
}
class Expression {
    std::string_view text_;
    const Constants &constants_;
    size_t pos_ = 0, depth_ = 0;
    void space() { while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t')) ++pos_; }
    bool take(std::string_view token) {
        space();
        if (!text_.substr(pos_).starts_with(token)) return false;
        pos_ += token.size(); return true;
    }
    int64_t atom() {
        if (++depth_ > 64) throw Error("expression nesting limit exceeded");
        int64_t result;
        space();
        if (pos_ < text_.size() && text_[pos_] == '\'') {
            auto [end, value] = character(text_, pos_); pos_ = end; result = value;
        }
        else if (take("(")) { result = expr(1); if (!take(")")) throw Error("expected ')' in expression"); }
        else if (take("~")) result = ~atom();
        else if (take("!")) result = !atom();
        else if (take("+")) result = atom();
        else if (take("-")) {
            // Parse the magnitude separately to admit INT64_MIN without overflow.
            space();
            size_t start = pos_;
            while (pos_ < text_.size() && ((text_[pos_] >= '0' && text_[pos_] <= '9') ||
                   (text_[pos_] >= 'a' && text_[pos_] <= 'f') || (text_[pos_] >= 'A' && text_[pos_] <= 'F') ||
                   text_[pos_] == 'x' || text_[pos_] == 'o')) ++pos_;
            if (start < pos_ && text_[start] >= '0' && text_[start] <= '9') {
                auto magnitude = std::string(text_.substr(start, pos_ - start));
                if (magnitude.size() > 1 && magnitude[0] == '0' && magnitude[1] >= '0' && magnitude[1] <= '9')
                    magnitude = "0o" + magnitude.substr(1);
                result = mair::detail::integer("-" + magnitude);
            }
            else { pos_ = start; result = negate(atom()); }
        } else {
            size_t start = pos_;
            while (pos_ < text_.size()) {
                char c = text_[pos_];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || c == '$')) break;
                ++pos_;
            }
            auto token = std::string(text_.substr(start, pos_ - start));
            if (token.empty()) throw Error("expected absolute integer expression");
            if (token.front() >= '0' && token.front() <= '9') {
                if (token.size() > 1 && token[0] == '0' && token[1] >= '0' && token[1] <= '9') token = "0o" + token.substr(1);
                result = mair::detail::integer(token);
            } else {
                auto it = constants_.find(token);
                if (it == constants_.end()) throw NonAbsolute("not an absolute constant: " + token);
                result = it->second;
            }
        }
        --depth_; return result;
    }
    static int64_t apply(std::string_view op, int64_t a, int64_t b) {
        if (op == "+") return add(a, b);
        if (op == "-") {
            if (b == INT64_MIN) { if (a >= 0) throw Error("integer expression overflow"); return a - b; }
            return add(a, -b);
        }
        if (op == "*") return multiply(a, b);
        if (op == "/" || op == "%") {
            if (!b) throw Error("division by zero in expression");
            if (a == INT64_MIN && b == -1) throw Error("integer expression overflow");
            return op == "/" ? a / b : a % b;
        }
        if (op == "<<" || op == ">>") {
            if (b < 0 || b > 63) throw Error("shift count outside 0..63");
            if (op == ">>") return a >> unsigned(b); // Defined arithmetic shift in C++20.
            for (int64_t i = 0; i < b; ++i) a = add(a, a);
            return a;
        }
        if (op == "&") return a & b;
        if (op == "|") return a | b;
        if (op == "^") return a ^ b;
        if (op == "==") return a == b;
        if (op == "!=") return a != b;
        if (op == "<=") return a <= b;
        if (op == ">=") return a >= b;
        if (op == "<") return a < b;
        if (op == ">") return a > b;
        if (op == "&&") return a && b;
        return a || b;
    }
    int64_t expr(int minimum) {
        int64_t a = atom();
        static constexpr std::pair<std::string_view, int> ops[] = {
            {"||", 1}, {"&&", 2}, {"==", 6}, {"!=", 6}, {"<=", 7}, {">=", 7},
            {"<<", 8}, {">>", 8}, {"|", 3}, {"^", 4}, {"&", 5}, {"<", 7}, {">", 7},
            {"+", 9}, {"-", 9}, {"*", 10}, {"/", 10}, {"%", 10}};
        for (;;) {
            space();
            std::string_view op;
            int precedence = 0;
            for (auto [candidate, p] : ops) if (text_.substr(pos_).starts_with(candidate)) {
                op = candidate; precedence = p; break;
            }
            if (precedence < minimum) return a;
            pos_ += op.size();
            a = apply(op, a, expr(precedence + 1));
        }
    }
public:
    Expression(std::string_view text, const Constants &constants) : text_(text), constants_(constants) {}
    int64_t run() {
        auto value = expr(1); space();
        if (pos_ != text_.size()) throw Error("unexpected text in integer expression");
        return value;
    }
};
} // namespace
int64_t evaluate(std::string_view text, const Constants &constants) { return Expression(text, constants).run(); }
} // namespace archas::detail
