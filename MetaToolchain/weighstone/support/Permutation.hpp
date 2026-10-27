#pragma once
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace weighstone::support {
inline void validate_permutation(const std::vector<std::size_t>& order,std::size_t size) {
    if(order.size()!=size) throw std::invalid_argument("permutation rank mismatch");
    std::vector<bool> seen(size,false);
    for(auto axis:order) {
        if(axis>=size || seen[axis]) throw std::invalid_argument("invalid permutation");
        seen[axis]=true;
    }
}
}
