#include <Weighstone/CCINT.hpp>
#include <stdexcept>
#include <iostream>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("test failed: " #condition); } while (false)
using namespace weighstone::ccint;
int main() {
    // Exhaustive fixed-width arithmetic at small widths.
    for (unsigned w=1; w<=5; ++w) {
        const unsigned mod=1u<<w;
        const long long min=-(1LL<<(w-1)), max=(1LL<<(w-1))-1;
        for (unsigned i=0;i<mod;++i) for(unsigned j=0;j<mod;++j) {
            BitVector a(w,i), b(w,j);
            const long long ai=i<mod/2 ? static_cast<long long>(i) : static_cast<long long>(i)-mod;
            const long long bj=j<mod/2 ? static_cast<long long>(j) : static_cast<long long>(j)-mod;
            CHECK(a.signed_value()==ai && b.signed_value()==bj);
            CHECK(a.sign_bit()==(ai<0) && b.sign_bit()==(bj<0));
            auto plus=checked_add(a,b), minus=checked_sub(a,b), mul=checked_mul(a,b);
            CHECK(plus.value.unsigned_value()==(i+j)%mod);
            CHECK(minus.value.unsigned_value()==(i+mod-j)%mod);
            CHECK(mul.value.unsigned_value()==(i*j)%mod);
            CHECK(plus.unsigned_overflow==(i+j>=mod));
            CHECK(minus.unsigned_overflow==(i<j));
            CHECK(mul.unsigned_overflow==(i*j>=mod));
            CHECK(plus.signed_overflow==(ai+bj<min||ai+bj>max));
            CHECK(minus.signed_overflow==(ai-bj<min||ai-bj>max));
            CHECK(mul.signed_overflow==(ai*bj<min||ai*bj>max));
            CHECK(KnownBits::constant(a).bit_and(KnownBits::constant(b)).constant_value()==(a&b));
            CHECK(KnownBits::constant(a).bit_or(KnownBits::constant(b)).constant_value()==(a|b));
            CHECK(KnownBits::constant(a).bit_xor(KnownBits::constant(b)).constant_value()==(a^b));
        }
    }
    for (long long a=-13;a<=13;++a) for(long long b=-7;b<=7;++b) if(b!=0) {
        BigInt q=floor_div(BigInt(a),BigInt(b));
        BigInt r=BigInt(a)-q*BigInt(b);
        CHECK((b>0 && r>=0 && r<b)||(b<0 && r<=0 && r>b));
    }
    std::cout << "Exhaustive properties passed\n";
}
