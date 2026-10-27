#pragma once
#include "BigInt.hpp"
#include <memory>
#include <string>
#include <stdexcept>
namespace weighstone::ccint {
class Expr {
public:
    enum class Kind { constant, symbol, add, mul, neg };
private:
    struct Node {
        Kind kind;
        BigInt value{};
        std::string name;
        std::shared_ptr<const Node> lhs, rhs;
    };
    std::shared_ptr<const Node> p_;
    explicit Expr(std::shared_ptr<const Node> p):p_(std::move(p)){}
    static bool equal(const std::shared_ptr<const Node>& a,const std::shared_ptr<const Node>& b) {
        if(a==b) return true;
        if(!a||!b||a->kind!=b->kind||a->value!=b->value||a->name!=b->name) return false;
        return equal(a->lhs,b->lhs)&&equal(a->rhs,b->rhs);
    }
    static Expr binary(Kind k,const Expr& a,const Expr& b) {
        return Expr(std::make_shared<Node>(Node{k,0,{},a.p_,b.p_}));
    }
public:
    explicit Expr(int v): Expr(BigInt(v)) {}
    explicit Expr(BigInt v):p_(std::make_shared<Node>(Node{Kind::constant,std::move(v),{},nullptr,nullptr})){}
    static Expr symbol(std::string name) {
        if(name.empty()) throw std::invalid_argument("empty symbol name");
        return Expr(std::make_shared<Node>(Node{Kind::symbol,0,std::move(name),nullptr,nullptr}));
    }
    Kind kind() const {return p_->kind;}
    bool is_constant() const {return kind()==Kind::constant;}
    const BigInt& constant() const { if(!is_constant()) throw std::logic_error("nonconstant"); return p_->value; }
    bool equivalent_tree(const Expr& b) const {return equal(p_,b.p_);}
    Expr simplify() const {
        if(kind()==Kind::constant||kind()==Kind::symbol) return *this;
        Expr a(p_->lhs); Expr x=a.simplify();
        if(kind()==Kind::neg) {
            if(x.is_constant()) return Expr(-x.constant());
            if(x.kind()==Kind::neg) return Expr(x.p_->lhs);
            return Expr(std::make_shared<Node>(Node{Kind::neg,0,{},x.p_,nullptr}));
        }
        Expr b(p_->rhs); Expr y=b.simplify();
        if(kind()==Kind::add) {
            if(x.is_constant() && y.is_constant()) return Expr(x.constant()+y.constant());
            if(x.is_constant()&&x.constant()==0) return y;
            if(y.is_constant()&&y.constant()==0) return x;
            if(y.kind()==Kind::neg && x.equivalent_tree(Expr(y.p_->lhs))) return Expr(BigInt(0));
            if(x.kind()==Kind::neg && y.equivalent_tree(Expr(x.p_->lhs))) return Expr(BigInt(0));
        } else if(kind()==Kind::mul) {
            if(x.is_constant()&&y.is_constant()) return Expr(x.constant()*y.constant());
            if((x.is_constant()&&x.constant()==0)||(y.is_constant()&&y.constant()==0)) return Expr(BigInt(0));
            if(x.is_constant()&&x.constant()==1) return y;
            if(y.is_constant()&&y.constant()==1) return x;
        }
        return binary(kind(),x,y);
    }
    friend Expr operator+(const Expr& a,const Expr& b) {return binary(Kind::add,a,b);}
    friend Expr operator*(const Expr& a,const Expr& b) {return binary(Kind::mul,a,b);}
    Expr operator-() const {return Expr(std::make_shared<Node>(Node{Kind::neg,0,{},p_,nullptr}));}
    friend Expr operator-(const Expr& a,const Expr& b) {return a+(-b);}
    std::string str() const {
        switch(kind()) {
        case Kind::constant:return p_->value.convert_to<std::string>();
        case Kind::symbol:return p_->name;
        case Kind::neg:return "(-"+Expr(p_->lhs).str()+")";
        case Kind::add:return "("+Expr(p_->lhs).str()+"+"+Expr(p_->rhs).str()+")";
        case Kind::mul:return "("+Expr(p_->lhs).str()+"*"+Expr(p_->rhs).str()+")";
        }
        throw std::logic_error("invalid expression kind");
    }
};
}
