/// Immutable arbitrary-precision integers, mirrored by bignum/Bignum.hpp.
module metalib.d.Bignum;
import metalib.d.Support;
import std.bigint : BigInt;
import std.conv : to;

private class Number : ForeignObj {
    BigInt value;
    this(BigInt n) { super("BigInt"); value = n; }
}
private Value wrap(BigInt n) { return wrap_handle(new Number(n)); }
private BigInt get(Value v, C c) {
    auto p = cast(Number)v.obj;
    if (v.kind != VK.Foreign || p is null) c.error("M0404", "expected a BigInt");
    return p.value;
}
void installBignum(Session s) {
    if (s.find_primitive("Bignum.parse") !is null) return;
    s.declare("type BigInt\n", "<metalib Bignum>");
    prim(s, "Bignum.parse", "String -> Result BigInt String", 1, (C c, V a) {
        auto text = S(a[0]); size_t i; bool negative;
        if (text.length && (text[0] == '-' || text[0] == '+')) { negative = text[0] == '-'; ++i; }
        if (i == text.length) return err("Bignum: expected decimal digits");
        BigInt n = BigInt(0);
        for (; i < text.length; ++i) {
            if (text[i] < '0' || text[i] > '9') return err("Bignum: expected decimal digits");
            n *= 10; n += text[i] - '0';
        }
        if (negative) n = -n;
        return ok(wrap(n));
    });
    prim(s, "Bignum.of_int", "Int -> BigInt", 1, (C c, V a) => wrap(BigInt(a[0].i)));
    prim(s, "Bignum.to_string", "BigInt -> String", 1, (C c, V a) => str(get(a[0], c).to!string));
    prim(s, "Bignum.to_int", "BigInt -> Option Int", 1, (C c, V a) {
        auto n = get(a[0], c);
        if (n < BigInt(long.min) || n > BigInt(long.max)) return none();
        return some(integer(n.toLong()));
    });
    prim(s, "Bignum.add", "BigInt -> BigInt -> BigInt", 2, (C c, V a) => wrap(get(a[0], c) + get(a[1], c)));
    prim(s, "Bignum.sub", "BigInt -> BigInt -> BigInt", 2, (C c, V a) => wrap(get(a[0], c) - get(a[1], c)));
    prim(s, "Bignum.mul", "BigInt -> BigInt -> BigInt", 2, (C c, V a) => wrap(get(a[0], c) * get(a[1], c)));
    prim(s, "Bignum.neg", "BigInt -> BigInt", 1, (C c, V a) => wrap(-get(a[0], c)));
    prim(s, "Bignum.compare", "BigInt -> BigInt -> Int", 2, (C c, V a) {
        auto x = get(a[0], c), y = get(a[1], c); return integer(x < y ? -1 : x > y ? 1 : 0);
    });
    prim(s, "Bignum.divmod", "BigInt -> BigInt -> Result (BigInt, BigInt) String", 2, (C c, V a) {
        auto x = get(a[0], c), y = get(a[1], c);
        if (y == 0) return err("Bignum: division by zero");
        return ok(pair(wrap(x / y), wrap(x % y)));
    });
}
