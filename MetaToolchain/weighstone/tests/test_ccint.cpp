#include <Weighstone/CCINT.hpp>
#include <stdexcept>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("test failed: " #condition); } while (false)
#include <iostream>
using namespace weighstone::ccint;
int main() {
    CHECK(gcd(BigInt(-24),BigInt(18))==6);
    CHECK(floor_div(BigInt(-7),BigInt(3))==-3);
    CHECK(ceil_div(BigInt(-7),BigInt(3))==-2);
    CHECK(euclidean_mod(BigInt(-7),BigInt(3))==2);
    CHECK(Rational(2,4)==Rational(1,2));
    CHECK((Rational(1,2)+Rational(1,3))==Rational(5,6));
    CHECK(extended_gcd(99,78).gcd==3);
    CHECK(mod_inverse(3,11).value()==4);
    CHECK(!chinese_remainder(0,2,1,2));
    auto crt=chinese_remainder(2,3,3,5); CHECK(crt && crt->residue==8);
    BitVector a(8,255), b(8,1);
    CHECK((a+b).unsigned_value()==0 && checked_add(a,b).unsigned_overflow);
    CHECK(a.signed_value()==-1 && !a.signed_add_overflow(b));
    CHECK(BitVector(129,-1).unsigned_value()==power_of_two(129)-1);
    CHECK(BitVector(8,128).ashr(2).signed_value()==-32);
    CHECK(BitVector(8,129).rol(1).unsigned_value()==3);
    CHECK(KnownBits::constant(BitVector(8,0xaa)).bit_and(KnownBits::constant(BitVector(8,0x0f))).constant_value().unsigned_value()==10);
    Interval i(0,10), j(5,20);
    CHECK(i.meet(j).lower().value()==5 && i.join(j).upper().value()==20);
    CHECK(i.mul(Interval(-2,3)).lower().value()==-20);
    CHECK(i.meet(Interval(11,12)).empty());
    auto con=Congruence(2,4).meet(Congruence(4,6)); CHECK(con && con->contains(10));
    CHECK(!Congruence(0,2).meet(Congruence(1,2)));
    AbstractInteger ar(Interval(0,10),Congruence(1,2));
    CHECK(ar.interval().lower().value()==1 && ar.interval().upper().value()==9);
    IntegerMatrix m({{1,2,3},{0,4,5},{1,0,6}}); CHECK(m.determinant()==22);
    auto x=LinearExpr::variable("x");
    VariableRanges vars{{"x",Interval(0,10)}};
    CHECK(check({x.scale(3)+LinearExpr(1),Relation::less_equal,31},vars).is_proven());
    CHECK(check({x,Relation::greater_equal,11},vars).is_disproven());
    const AffineRecurrence rec{1,3}; CHECK(rec.trip_count_less_than(11).value()==4);
    auto sym=Expr::symbol("x");
    CHECK(prove_equal(sym+(-sym),Expr(BigInt(0))).is_proven());
    CHECK(!prove_equal(sym*Expr(2),sym).is_proven());
    std::cout << "CCINT tests passed\n";
}
