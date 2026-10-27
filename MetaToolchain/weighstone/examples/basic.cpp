#include <Weighstone/CCINT.hpp>
#include <iostream>
int main() {
    using namespace weighstone::ccint;
    const auto range=Interval(2,8).mul(Interval(3,4));
    std::cout << "product range: [" << *range.lower() << ", " << *range.upper() << "]\n";
    const auto x=BitVector(8,255);
    std::cout << "8-bit wrapped increment: " << (x+BitVector(8,1)).unsigned_value() << "\n";
}
