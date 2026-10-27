#pragma once
#include "Affine.hpp"
#include "../../../support/Permutation.hpp"

namespace weighstone::ccint {
// Metadata only: exact extents/offsets, with no host allocation or data ownership.
class TensorShape {
    std::vector<BigInt> extents_;
public:
    explicit TensorShape(std::vector<BigInt> extents) : extents_(std::move(extents)) {
        for(const auto& extent:extents_) if(extent<0) throw std::invalid_argument("negative tensor extent");
    }
    const auto& extents() const noexcept { return extents_; }
    std::size_t rank() const noexcept { return extents_.size(); }
    BigInt elements() const {
        BigInt count=1;
        for(const auto& extent:extents_) count*=extent;
        return count; // rank zero is a scalar, with one element
    }
    TensorShape broadcast(const TensorShape& other) const {
        std::vector<BigInt> result(std::max(rank(),other.rank()),1);
        for(std::size_t i=0;i<result.size();++i) {
            const BigInt a=i<rank()?extents_[rank()-1-i]:BigInt(1);
            const BigInt b=i<other.rank()?other.extents_[other.rank()-1-i]:BigInt(1);
            if(a!=b && a!=1 && b!=1) throw std::invalid_argument("incompatible broadcast shapes");
            result[result.size()-1-i]=a==1?b:a; // 0 broadcast with 1 remains 0
        }
        return TensorShape(std::move(result));
    }
    friend bool operator==(const TensorShape&,const TensorShape&)=default;
};

class TensorLayout {
    TensorShape shape_;
    std::vector<BigInt> strides_;
    BigInt base_;
public:
    // Strides and base are in elements, not bytes. Negative/zero strides are valid.
    TensorLayout(TensorShape shape,std::vector<BigInt> strides,BigInt base=0)
        : shape_(std::move(shape)),strides_(std::move(strides)),base_(std::move(base)) {
        if(shape_.rank()!=strides_.size()) throw std::invalid_argument("tensor stride rank mismatch");
    }
    static TensorLayout row_major(TensorShape shape,BigInt base=0) {
        std::vector<BigInt> strides(shape.rank());
        BigInt stride=1;
        for(std::size_t i=shape.rank();i>0;--i) {
            strides[i-1]=stride; stride*=shape.extents()[i-1];
        }
        return {std::move(shape),std::move(strides),std::move(base)};
    }
    const TensorShape& shape() const noexcept { return shape_; }
    const auto& strides() const noexcept { return strides_; }
    const BigInt& base() const noexcept { return base_; }
    BigInt offset(const std::vector<BigInt>& indices) const {
        if(indices.size()!=shape_.rank()) throw std::invalid_argument("tensor index rank mismatch");
        BigInt result=base_;
        for(std::size_t i=0;i<indices.size();++i) {
            if(indices[i]<0 || indices[i]>=shape_.extents()[i]) throw std::out_of_range("tensor index outside extent");
            result+=indices[i]*strides_[i];
        }
        return result;
    }
    // Inclusive hull of touched element offsets; padding can leave holes.
    Interval storage_range() const {
        if(shape_.elements()==0) return Interval::bottom();
        BigInt lo=base_, hi=base_;
        for(std::size_t i=0;i<shape_.rank();++i) {
            const BigInt delta=(shape_.extents()[i]-1)*strides_[i];
            if(delta<0) lo+=delta; else hi+=delta;
        }
        return {lo,hi};
    }
    bool is_row_major_contiguous() const {
        if(shape_.elements()==0) return true;
        BigInt expected=1;
        for(std::size_t i=shape_.rank();i>0;--i) {
            if(shape_.extents()[i-1]>1 && strides_[i-1]!=expected) return false;
            expected*=shape_.extents()[i-1];
        }
        return true;
    }
    // order[new_axis] = old_axis; also used by loop permutation APIs.
    TensorLayout permute(const std::vector<std::size_t>& order) const {
        weighstone::support::validate_permutation(order,shape_.rank());
        std::vector<BigInt> extents,strides;
        for(auto i:order) {extents.push_back(shape_.extents()[i]); strides.push_back(strides_[i]);}
        return {TensorShape(std::move(extents)),std::move(strides),base_};
    }
    TensorLayout broadcast_to(TensorShape target) const {
        if(target.rank()<shape_.rank()) throw std::invalid_argument("broadcast reduces rank");
        std::vector<BigInt> strides(target.rank(),0);
        const auto leading=target.rank()-shape_.rank();
        for(std::size_t i=0;i<shape_.rank();++i) {
            if(shape_.extents()[i]==target.extents()[leading+i]) strides[leading+i]=strides_[i];
            else if(shape_.extents()[i]!=1) throw std::invalid_argument("incompatible broadcast extent");
        }
        return {std::move(target),std::move(strides),base_};
    }
    TensorLayout reshape(TensorShape target) const {
        if(target.elements()!=shape_.elements()) throw std::invalid_argument("reshape changes element count");
        if(!is_row_major_contiguous()) throw std::invalid_argument("reshape requires row-major contiguous layout");
        return row_major(std::move(target),base_);
    }
    AffineMap linearize(const AffineMap& access) const {
        if(access.outputs().size()!=shape_.rank()) throw std::invalid_argument("tensor access rank mismatch");
        LinearExpr result(base_);
        for(std::size_t i=0;i<shape_.rank();++i) result+=access.outputs()[i].scale(strides_[i]);
        return {access.inputs(),{std::move(result)}};
    }
};

// Universal in-bounds proof over the integer iteration domain. A layout alone
// does not establish allocation validity, alias freedom, or safe parallel writes.
inline Verdict prove_in_bounds(const Polyhedron& domain,const AffineMap& access,
                               const TensorShape& shape,AnalysisBudget budget={}) {
    if(domain.variables()!=access.inputs() || access.outputs().size()!=shape.rank())
        throw std::invalid_argument("tensor access coordinate mismatch");
    if(domain.is_empty(budget).is_proven()) return {Decision::proven,"empty iteration domain"};
    bool unknown=false;
    for(std::size_t i=0;i<shape.rank();++i) {
        const auto lower=domain.prove({access.outputs()[i],Relation::greater_equal,0},budget);
        const auto upper=domain.prove({access.outputs()[i],Relation::less_equal,BigInt(shape.extents()[i]-1)},budget);
        if(lower.is_disproven() || upper.is_disproven()) return {Decision::disproven,"an index is always out of bounds"};
        unknown|=!lower.is_proven() || !upper.is_proven();
    }
    return unknown ? Verdict{Decision::unknown,"index bounds inconclusive"}
                   : Verdict{Decision::proven,"all tensor indices in bounds"};
}
}
