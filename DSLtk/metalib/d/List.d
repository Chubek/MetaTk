/**
 * metalib/d/List.d — MetaML host module `List` (D dialect): host-accelerated list utilities.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/List.h):
 *   (see metalib/cpp/List.h for the primitive list)
 */
module metalib.d.List;

import MetaML;
import metalib.d.Support;


private PrimFn extremePrim(bool want_max)
{
    return delegate Value(C c, V a) {
        auto xs = items(a[1]);
        if (!xs.length) return none();
        Value best = xs[0];
        foreach (i; 1 .. xs.length)
        {
            auto r = c.apply(a[0], [xs[i], best]).i;
            if (want_max ? r > 0 : r < 0) best = xs[i];
        }
        return some(best);
    };
}

void installList(Session s)
{
    prim(s, "List.take_while", "('a -> Bool) -> ['a] -> ['a]", 2, delegate Value(C c, V a) {
        Value[] result;
        foreach (x; items(a[1]))
        {
            if (!c.apply(a[0], [x]).i) break;
            result ~= x;
        }
        return list(result);
    });
    prim(s, "List.drop_while", "('a -> Bool) -> ['a] -> ['a]", 2, delegate Value(C c, V a) {
        Value[] result;
        bool dropping = true;
        foreach (x; items(a[1]))
        {
            if (dropping && c.apply(a[0], [x]).i) continue;
            dropping = false;
            result ~= x;
        }
        return list(result);
    });
    prim(s, "List.chunks", "Int -> ['a] -> [['a]]", 2, delegate Value(C c, V a) {
        import std.algorithm : min;
        if (a[0].i <= 0) c.error("M0404", "`List.chunks` expects a positive size");
        auto xs = items(a[1]);
        Value[] result;
        size_t n = cast(size_t) a[0].i;
        for (size_t i = 0; i < xs.length; i += n) result ~= list(xs[i .. min(xs.length, i + n)]);
        return list(result);
    });
    prim(s, "List.windows", "Int -> ['a] -> [['a]]", 2, delegate Value(C c, V a) {
        if (a[0].i <= 0) c.error("M0404", "`List.windows` expects a positive size");
        auto xs = items(a[1]);
        size_t n = cast(size_t) a[0].i;
        Value[] result;
        for (size_t i = 0; i + n <= xs.length; ++i) result ~= list(xs[i .. i + n]);
        return list(result);
    });
    prim(s, "List.group", "['a] -> [['a]]", 1, delegate Value(C c, V a) {
        Value[] result, cur;
        foreach (x; items(a[0]))
        {
            if (cur.length && !values_equal(cur[$ - 1], x, c.span))
            {
                result ~= list(cur);
                cur = null;
            }
            cur ~= x;
        }
        if (cur.length) result ~= list(cur);
        return list(result);
    });
    prim(s, "List.flatten", "[['a]] -> ['a]", 1, delegate Value(C c, V a) {
        Value[] result;
        foreach (xs; items(a[0])) foreach (x; items(xs)) result ~= x;
        return list(result);
    });
    prim(s, "List.index_of", "'a -> ['a] -> Option Int", 2, delegate Value(C c, V a) {
        long i = 0;
        foreach (x; items(a[1]))
        {
            if (values_equal(x, a[0], c.span)) return some(integer(i));
            ++i;
        }
        return none();
    });
    prim(s, "List.transpose", "[['a]] -> [['a]]", 1, delegate Value(C c, V a) {
        Value[][] rows;
        foreach (r; items(a[0])) rows ~= items(r);
        Value[] result;
        if (!rows.length) return list(result);
        size_t n = rows[0].length;
        foreach (r; rows) if (r.length < n) n = r.length;
        foreach (k; 0 .. n)
        {
            Value[] col;
            foreach (r; rows) col ~= r[k];
            result ~= list(col);
        }
        return list(result);
    });
    prim(s, "List.scan", "('b -> 'a -> 'b) -> 'b -> ['a] -> ['b]", 3, delegate Value(C c, V a) {
        Value[] result = [a[1]];
        Value acc = a[1];
        foreach (x; items(a[2]))
        {
            acc = c.apply(a[0], [acc, x]);
            result ~= acc;
        }
        return list(result);
    });
    prim(s, "List.iterate", "Int -> ('a -> 'a) -> 'a -> ['a]", 3, delegate Value(C c, V a) {
        Value[] result;
        Value x = a[2];
        for (long k = 0; k < a[0].i; ++k)
        {
            result ~= x;
            if (k + 1 < a[0].i) x = c.apply(a[1], [x]);
        }
        return list(result);
    });
    prim(s, "List.zip3", "['a] -> ['b] -> ['c] -> [('a, 'b, 'c)]", 3, delegate Value(C c, V a) {
        auto x = items(a[0]), y = items(a[1]), z = items(a[2]);
        Value[] result;
        for (size_t i = 0; i < x.length && i < y.length && i < z.length; ++i) result ~= triple(x[i], y[i], z[i]);
        return list(result);
    });
    prim(s, "List.rotate", "Int -> ['a] -> ['a]", 2, delegate Value(C c, V a) {
        auto xs = items(a[1]);
        if (!xs.length) return list(xs);
        long n = cast(long) xs.length;
        long k = ((a[0].i % n) + n) % n;
        return list(xs[cast(size_t) k .. $] ~ xs[0 .. cast(size_t) k]);
    });
    prim(s, "List.shuffle", "Int -> ['a] -> ['a]", 2, delegate Value(C c, V a) {
        auto xs = items(a[1]).dup;
        ulong st = cast(ulong) a[0].i ^ 0x9E3779B97F4A7C15UL;
        if (st == 0) st = 1;
        ulong next()
        {
            st ^= st >> 12;
            st ^= st << 25;
            st ^= st >> 27;
            return st * 2685821657736338717UL;
        }
        for (size_t i = xs.length; i > 1; --i)
        {
            size_t j = cast(size_t)(next() % i);
            auto t = xs[i - 1];
            xs[i - 1] = xs[j];
            xs[j] = t;
        }
        return list(xs);
    });
    prim(s, "List.max_by", "('a -> 'a -> Int) -> ['a] -> Option 'a", 2, extremePrim(true));
    prim(s, "List.min_by", "('a -> 'a -> Int) -> ['a] -> Option 'a", 2, extremePrim(false));
}

