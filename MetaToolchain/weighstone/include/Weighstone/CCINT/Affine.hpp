#pragma once
#include "Polyhedron.hpp"

namespace weighstone::ccint {
// Ordered affine outputs over a declared, ordered coordinate space.
class AffineMap {
    std::vector<std::string> inputs_;
    std::vector<LinearExpr> outputs_;
public:
    AffineMap(std::vector<std::string> inputs,std::vector<LinearExpr> outputs)
        : inputs_(std::move(inputs)),outputs_(std::move(outputs)) {
        const Polyhedron space(inputs_);
        for(const auto& out:outputs_) space.validate(out);
    }
    const auto& inputs() const noexcept { return inputs_; }
    const auto& outputs() const noexcept { return outputs_; }
    std::vector<BigInt> eval(const IntegerPoint& point) const {
        for(const auto& v:inputs_) if(!point.contains(v)) throw std::invalid_argument("missing affine map input");
        std::vector<BigInt> result;
        for(const auto& out:outputs_) result.push_back(*out.eval(point));
        return result;
    }
    // this(inner(x)); inner outputs correspond positionally to this inputs.
    AffineMap compose(const AffineMap& inner) const {
        if(inputs_.size()!=inner.outputs_.size()) throw std::invalid_argument("affine composition rank mismatch");
        std::vector<LinearExpr> result;
        for(const auto& out:outputs_) {
            LinearExpr expr(out.constant());
            for(std::size_t i=0;i<inputs_.size();++i) {
                auto term=out.terms().find(inputs_[i]);
                if(term!=out.terms().end()) expr+=inner.outputs_[i].scale(term->second);
            }
            result.push_back(std::move(expr));
        }
        return {inner.inputs_,std::move(result)};
    }
};
}
