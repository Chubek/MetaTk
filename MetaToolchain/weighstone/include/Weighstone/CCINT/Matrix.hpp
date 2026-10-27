#pragma once
#include "BigInt.hpp"
#include <vector>
#include <stdexcept>
namespace weighstone::ccint {
class IntegerMatrix {
    std::vector<std::vector<BigInt>> data_;
public:
    IntegerMatrix(std::vector<std::vector<BigInt>> data) : data_(std::move(data)) {
        if(!data_.empty()) for(const auto& row:data_) if(row.size()!=data_[0].size()) throw std::invalid_argument("ragged matrix");
    }
    size_t rows() const {return data_.size();}
    size_t cols() const {return data_.empty()?0:data_[0].size();}
    const BigInt& at(size_t r,size_t c) const {return data_.at(r).at(c);}
    // Fraction-free Bareiss determinant, exact for all integer square matrices.
    BigInt determinant() const {
        if(rows()!=cols()) throw std::invalid_argument("determinant requires square matrix");
        const size_t n=rows(); if(n==0) return 1;
        auto a=data_; BigInt prev=1; int sign=1;
        for(size_t k=0;k+1<n;++k) {
            if(a[k][k]==0) {
                size_t row=k+1; while(row<n && a[row][k]==0) ++row;
                if(row==n) return 0;
                std::swap(a[k],a[row]); sign=-sign;
            }
            const BigInt pivot=a[k][k];
            for(size_t i=k+1;i<n;++i) for(size_t j=k+1;j<n;++j)
                a[i][j]=(a[i][j]*pivot-a[i][k]*a[k][j])/prev;
            for(size_t i=k+1;i<n;++i) a[i][k]=0;
            prev=pivot;
        }
        return sign*a[n-1][n-1];
    }
};
}
