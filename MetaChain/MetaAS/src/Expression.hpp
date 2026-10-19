#pragma once
#include "Internal.hpp"
#include <charconv>
#include <functional>

namespace metaas::detail {
struct Value { std::int64_t number=0; std::string symbol; };
class Expression {
    std::string_view text_;
    std::size_t pos_=0, max_depth_;
    std::function<Value(std::string_view)> resolve_;
    std::function<std::int64_t(const Value&,const Value&)> difference_;
    void space() { while (pos_<text_.size() && (text_[pos_]==' ' || text_[pos_]=='\t')) ++pos_; }
    bool take(char c) { space(); if (pos_<text_.size() && text_[pos_]==c) { ++pos_; return true; } return false; }
    Value atom(std::size_t depth) {
        if (depth>max_depth_) throw std::invalid_argument("expression depth limit exceeded");
        space();
        if (take('+')) return atom(depth+1);
        if (take('~')) {
            auto v=atom(depth+1); if (!v.symbol.empty()) throw std::invalid_argument("bitwise symbolic expression is not representable");
            v.number=~v.number; return v;
        }
        // Parse the negative endpoint without first overflowing its magnitude.
        bool negative=take('-');
        if (negative) {
            space();
            if (pos_<text_.size() && text_[pos_]>='0' && text_[pos_]<='9') return integer(true);
            auto v=atom(depth+1);
            if (!v.symbol.empty()) throw std::invalid_argument("negative symbol is not representable");
            v.number=sub(0,v.number); return v;
        }
        if (take('(')) { auto v=expr(0,depth+1); if (!take(')')) throw std::invalid_argument("expected ')' in expression"); return v; }
        space();
        if (pos_==text_.size()) throw std::invalid_argument("expected expression");
        if (text_[pos_]>='0' && text_[pos_]<='9') return integer(false);
        if (first(text_[pos_])) {
            auto start=pos_++;
            while (pos_<text_.size() && rest(text_[pos_])) ++pos_;
            return resolve_(text_.substr(start,pos_-start));
        }
        throw std::invalid_argument("invalid expression token");
    }
    Value integer(bool negative) {
        auto start=pos_; int base=10;
        if (pos_+1<text_.size() && text_[pos_]=='0') {
            char c=text_[pos_+1];
            if (c=='x' || c=='X') base=16;
            else if (c=='b' || c=='B') base=2;
            else if (c=='o' || c=='O') base=8;
            if (base!=10) pos_+=2;
        }
        const auto begin=pos_;
        while (pos_<text_.size() && ((text_[pos_]>='0'&&text_[pos_]<='9') ||
               (base==16 && ((text_[pos_]>='a'&&text_[pos_]<='f') || (text_[pos_]>='A'&&text_[pos_]<='F'))))) ++pos_;
        std::uint64_t n=0;
        auto parsed=std::from_chars(text_.data()+begin,text_.data()+pos_,n,base);
        if (begin==pos_ || parsed.ec!=std::errc{} || parsed.ptr!=text_.data()+pos_ ||
            (negative && n>(UINT64_C(1)<<63)) || (!negative && base==10 && n>INT64_MAX))
            throw std::invalid_argument("integer literal out of range near " + std::string(text_.substr(start,pos_-start)));
        return {std::bit_cast<std::int64_t>(negative ? UINT64_C(0)-n : n),{}};
    }
    std::pair<std::string_view,int> op() {
        space(); if (pos_==text_.size()) return {{},-1};
        auto tail=text_.substr(pos_);
        if (tail.starts_with("<<") || tail.starts_with(">>")) return {tail.substr(0,2),3};
        char c=tail[0];
        return {tail.substr(0,1),c=='|' ? 0 : c=='^' ? 1 : c=='&' ? 2 :
                (c=='+'||c=='-') ? 4 : (c=='*'||c=='/'||c=='%') ? 5 : -1};
    }
    Value combine(Value a,std::string_view op,Value b) {
        if (!a.symbol.empty() || !b.symbol.empty()) {
            if (op=="+" && (a.symbol.empty() || b.symbol.empty())) return {add(a.number,b.number),a.symbol.empty()?b.symbol:a.symbol};
            if (op=="-" && b.symbol.empty()) return {sub(a.number,b.number),a.symbol};
            if (op=="-" && !a.symbol.empty() && !b.symbol.empty()) return {difference_(a,b),{}};
            throw std::invalid_argument("symbolic expression must be symbol + addend or a same-section difference");
        }
        auto x=a.number,y=b.number;
        if (op=="+") return {add(x,y),{}};
        if (op=="-") return {sub(x,y),{}};
        if (op=="*") {
            if ((x>0 && ((y>0 && x>INT64_MAX/y) || (y<0 && y<INT64_MIN/x))) ||
                (x<0 && ((y>0 && x<INT64_MIN/y) || (y<0 && x<INT64_MAX/y))))
                throw std::invalid_argument("integer multiplication overflow");
            return {x*y,{}};
        }
        if (op=="/" || op=="%") {
            if (!y || (x==INT64_MIN && y==-1)) throw std::invalid_argument("invalid division or remainder");
            return {op=="/" ? x/y : x%y,{}};
        }
        if (op=="<<" || op==">>") {
            if (y<0 || y>63) throw std::invalid_argument("shift count outside 0..63");
            auto bits=std::bit_cast<std::uint64_t>(x);
            return {std::bit_cast<std::int64_t>(op=="<<" ? bits<<y : bits>>y),{}};
        }
        return {op=="&" ? x&y : op=="|" ? x|y : x^y,{}};
    }
    Value expr(int precedence,std::size_t depth) {
        auto left=atom(depth);
        for (;;) {
            auto [token,level]=op(); if (level<precedence) break;
            pos_+=token.size();
            left=combine(std::move(left),token,expr(level+1,depth+1));
        }
        return left;
    }
public:
    Expression(std::string_view text,std::size_t depth,std::function<Value(std::string_view)> resolve,
               std::function<std::int64_t(const Value&,const Value&)> difference)
        :text_(text),max_depth_(depth),resolve_(std::move(resolve)),difference_(std::move(difference)) {}
    Value run() {
        auto v=expr(0,0); space();
        if (pos_!=text_.size()) throw std::invalid_argument("unexpected trailing expression text");
        return v;
    }
};
} // namespace metaas::detail
