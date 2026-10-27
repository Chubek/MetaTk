#include <Weighstone/CCINT.hpp>
#include <iostream>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("test failed: " #condition); } while (false)
using namespace weighstone::ccint;
template<class Exception,class F> void throws(F f) {
    bool caught=false;
    try {f();} catch(const Exception&) {caught=true;}
    CHECK(caught);
}

static void core_regressions() {
    for(unsigned bit:{0u,1u,63u,64u,257u,1048576u}) {
        const BigInt power=power_of_two(bit);
        CHECK(boost::multiprecision::msb(power)==bit);
        CHECK((power&(power-1))==0);
    }
    // Materialized masks must survive large temporary Boost allocations.
    const BigInt high=power_of_two(256);
    const KnownBits bits(257,high,7);
    CHECK(bits.known_zero()==high && bits.known_one()==7);
    throws<std::invalid_argument>([]{KnownBits invalid(1048577);});
    throws<std::invalid_argument>([]{KnownBits invalid(4,1,1);});
    throws<std::invalid_argument>([]{BitVector(8,3).trunc(9);});
    CHECK(BitVector(8,255).trunc(4).unsigned_value()==15);
    CHECK(BitVector(8,255).trunc(8)==BitVector(8,255));

    AbstractInteger excluded(Interval::lower_bounded(4),Congruence::point(3));
    CHECK(excluded.empty() && excluded.interval().empty());
    AbstractInteger exact({},Congruence::point(-7));
    CHECK(exact.interval()==Interval::point(-7));
    CHECK(AbstractInteger(Interval::lower_bounded(-4),Congruence(1,3)).interval()==Interval::lower_bounded(-2));
    CHECK(AbstractInteger(Interval::upper_bounded(-4),Congruence(1,3)).interval()==Interval::upper_bounded(-5));
    CHECK(AbstractInteger(Interval(0,1),Congruence(2,3)).interval().empty());
    CHECK(AbstractInteger({},Congruence(0,2)).meet(AbstractInteger({},Congruence(1,2))).empty());
    CHECK(AbstractInteger(Interval(0,10),Congruence(0,2)).meet(
        AbstractInteger(Interval(3,9),Congruence(1,3))).interval()==Interval::point(4));

    CHECK(Interval::lower_bounded(3).scale(-2)==Interval::upper_bounded(-6));
    CHECK(Interval{}.scale(0)==Interval::point(0));
    CHECK(Interval::bottom().scale(0).empty());
    CHECK(Interval(0,4).widen(Interval(-1,6))==Interval{});
    CHECK(Interval(0,4).widen(Interval(1,3))==Interval(0,4));
    CHECK(Interval::bottom().widen(Interval(0,1))==Interval(0,1));
    CHECK(Interval(0,1).widen(Interval::bottom())==Interval(0,1));
    CHECK(Interval::lower_bounded(0).widen(Interval(1,9))==Interval::lower_bounded(0));
    const auto x=LinearExpr::variable("x");
    CHECK(x.scale(-3).range({{"x",Interval::lower_bounded(2)}})==Interval::upper_bounded(-6));
    CHECK(LinearExpr(7).range({{"unused",Interval::bottom()}}).empty());
    CHECK(check({LinearExpr(7),Relation::equal,0},{{"unused",Interval::bottom()}}).is_proven());
    throws<std::invalid_argument>([]{LinearExpr::variable("");});
    CHECK((AffineRecurrence{3,0}.trip_count_less_than(2)==BigInt(0)));
    CHECK((!AffineRecurrence{3,0}.trip_count_less_than(4)));
    CHECK((AffineRecurrence{3,-2}.trip_count_greater_equal(-1)==BigInt(3)));
    CHECK((AffineRecurrence{3,2}.trip_count_less_equal(3)==BigInt(1)));

    // Lattice and reduction soundness over independently enumerated integers.
    for(int lo=-4;lo<=4;++lo) for(int hi=lo;hi<=4;++hi)
        for(int r=-3;r<=3;++r) for(int m=0;m<=4;++m) {
            const Interval interval(lo,hi); const Congruence congruence(r,m);
            const AbstractInteger product(interval,congruence);
            bool any=false;
            for(int value=-5;value<=5;++value) {
                const bool member=interval.contains(value) && congruence.contains(value);
                CHECK(member==(!product.empty() && product.interval().contains(value) && product.congruence().contains(value)));
                any|=member;
            }
            CHECK(product.empty()==!any);
        }
}

static void polyhedra() {
    const auto x=LinearExpr::variable("x"), y=LinearExpr::variable("y");
    Polyhedron triangle({"x","y"});
    triangle.add({x,Relation::greater_equal,0}).add({x,Relation::less_equal,4});
    triangle.add({y,Relation::greater_equal,0}).add({y,Relation::less_equal,4});
    triangle.add({x+y,Relation::less_equal,4});
    const auto points=triangle.integer_points();
    CHECK(points.complete.is_proven() && points.points.size()==15);
    CHECK(triangle.contains({{"x",1},{"y",3}}));
    CHECK(!triangle.contains({{"x",3},{"y",3}}));
    CHECK(triangle.prove({x+y,Relation::less_equal,4}).is_proven());
    CHECK(triangle.prove({x+y,Relation::greater_equal,5}).is_disproven());
    CHECK(triangle.prove({x,Relation::equal,2}).decision==Decision::unknown);
    CHECK(triangle.integer_points({24,4096}).complete.decision==Decision::unknown);
    CHECK(triangle.integer_points({25,4096}).complete.is_proven());

    Polyhedron impossible({"x","y"});
    impossible.add({x-y,Relation::less_equal,0}).add({y-x,Relation::less_equal,-1});
    CHECK(impossible.is_empty().is_proven());
    CHECK(impossible.is_empty({0,0}).decision==Decision::unknown);
    CHECK(impossible.prove({x,Relation::equal,999}).is_proven());
    Polyhedron parity({"x","y"});
    parity.add({(x+y).scale(2),Relation::equal,1});
    CHECK(parity.is_empty().is_proven()); // rational feasibility is insufficient
    Polyhedron unknown({"x","y"});
    unknown.add({x-y,Relation::greater_equal,1});
    CHECK(unknown.is_empty().decision==Decision::unknown);
    CHECK(Polyhedron({"x"}).is_empty({0,0}).is_disproven()); // concrete zero witness
    Polyhedron zero_dim({});
    CHECK(zero_dim.integer_points().points.size()==1);
    CHECK(zero_dim.integer_points({0,0}).complete.decision==Decision::unknown);
    zero_dim.add({LinearExpr(1),Relation::equal,0});
    CHECK(zero_dim.is_empty().is_proven());
    CHECK(zero_dim.integer_points({0,0}).points.empty());

    Polyhedron negative({"x"});
    negative.add({x.scale(-3),Relation::less_equal,7}).add({x,Relation::less_equal,0});
    CHECK(negative.bounding_box().at("x")==Interval(-2,0));
    CHECK(negative.integer_points().points.size()==3);
    const BigInt huge=power_of_two(150);
    Polyhedron large({"x"});
    large.add({x,Relation::greater_equal,huge}).add({x,Relation::less_equal,BigInt(huge+1)});
    CHECK(large.integer_points().points.size()==2);
    CHECK(large.is_empty().is_disproven());
    CHECK(large.intersect(Polyhedron({"x"})).integer_points().points.size()==2);
    throws<std::invalid_argument>([]{Polyhedron invalid({"x","x"});});
    throws<std::invalid_argument>([&]{triangle.add({LinearExpr::variable("z"),Relation::equal,0});});
    throws<std::invalid_argument>([&]{triangle.contains({{"x",0}});});
    throws<std::invalid_argument>([&]{triangle.intersect(negative);});

    const AffineMap outer({"x","y"},{x.scale(2)-y+LinearExpr(3)});
    const auto t=LinearExpr::variable("t");
    const AffineMap inner({"t"},{t+LinearExpr(1),t.scale(3)});
    CHECK(outer.compose(inner).eval({{"t",7}})==std::vector<BigInt>{-2});
    CHECK(AffineMap({},{}).eval({}).empty());
    throws<std::invalid_argument>([&]{outer.eval({{"x",0}});});
    throws<std::invalid_argument>([&]{outer.compose(outer);});
}

static void polyhedron_soundness() {
    const auto x=LinearExpr::variable("x"),y=LinearExpr::variable("y");
    // Independent bounded oracle checks elimination, integer rounding and all
    // verdicts on thousands of correlated two-variable systems.
    for(int a=-2;a<=2;++a) for(int b=-2;b<=2;++b)
        for(int c=-3;c<=3;++c) for(int d=-2;d<=2;++d) {
            Polyhedron p({"x","y"});
            p.add({x,Relation::greater_equal,-2}).add({x,Relation::less_equal,2});
            p.add({y,Relation::greater_equal,-2}).add({y,Relation::less_equal,2});
            p.add({x.scale(a)+y.scale(b),Relation::less_equal,c});
            p.add({x-y,Relation::equal,d});
            std::size_t count=0;
            bool all=true, none=true;
            for(int i=-2;i<=2;++i) for(int j=-2;j<=2;++j)
                if(a*i+b*j<=c && i-j==d) {++count; all&=i+j<=0; none&=i+j>0;}
            const auto empty=p.is_empty();
            CHECK(empty.is_proven()==(count==0));
            CHECK(empty.is_disproven()==(count!=0));
            const auto enumeration=p.integer_points();
            CHECK(enumeration.complete.is_proven() && enumeration.points.size()==count);
            const auto proof=p.prove({x+y,Relation::less_equal,0});
            CHECK(!proof.is_proven() || all);
            CHECK(!proof.is_disproven() || none);
            // With no enumeration budget, every positive emptiness result must
            // still be justified by the independently checked integer system.
            CHECK(!p.is_empty({0,4096}).is_proven() || count==0);
        }
}

int main() {
    core_regressions(); polyhedra(); polyhedron_soundness();
    std::cout << "Core and polyhedral analysis tests passed\n";
}
