// metalib/cpp/List.c — implementation of List.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// List.h and must not be compiled on its own. Mirror: metalib/d/List.d.
#pragma once

namespace metalib
{


inline void
install_List (metaml::Session &s)
{
  using namespace support;
  prim (s, "List.take_while", "('a -> Bool) -> ['a] -> ['a]", 2, [] (C &c, V &a) {
    std::vector<Value> out;
    for (auto &x : items (a[1]))
      {
        if (!c.apply (a[0], { x }).i)
          break;
        out.push_back (x);
      }
    return list (out);
  });
  prim (s, "List.drop_while", "('a -> Bool) -> ['a] -> ['a]", 2, [] (C &c, V &a) {
    std::vector<Value> out;
    bool dropping = true;
    for (auto &x : items (a[1]))
      {
        if (dropping && c.apply (a[0], { x }).i)
          continue;
        dropping = false;
        out.push_back (x);
      }
    return list (out);
  });
  prim (s, "List.chunks", "Int -> ['a] -> [['a]]", 2, [] (C &c, V &a) {
    if (a[0].i <= 0)
      c.error ("M0404", "`List.chunks` expects a positive size");
    auto xs = items (a[1]);
    std::vector<Value> out;
    for (std::size_t i = 0; i < xs.size (); i += static_cast<std::size_t> (a[0].i))
      {
        std::size_t e = std::min (xs.size (), i + static_cast<std::size_t> (a[0].i));
        out.push_back (list (std::vector<Value> (xs.begin () + static_cast<std::ptrdiff_t> (i),
                                                 xs.begin () + static_cast<std::ptrdiff_t> (e))));
      }
    return list (out);
  });
  prim (s, "List.windows", "Int -> ['a] -> [['a]]", 2, [] (C &c, V &a) {
    if (a[0].i <= 0)
      c.error ("M0404", "`List.windows` expects a positive size");
    auto xs = items (a[1]);
    std::size_t n = static_cast<std::size_t> (a[0].i);
    std::vector<Value> out;
    for (std::size_t i = 0; i + n <= xs.size (); ++i)
      out.push_back (list (std::vector<Value> (xs.begin () + static_cast<std::ptrdiff_t> (i),
                                               xs.begin () + static_cast<std::ptrdiff_t> (i + n))));
    return list (out);
  });
  prim (s, "List.group", "['a] -> [['a]]", 1, [] (C &c, V &a) {
    std::vector<Value> out, cur;
    for (auto &x : items (a[0]))
      {
        if (!cur.empty () && !metaml::values_equal (cur.back (), x, c.span))
          {
            out.push_back (list (cur));
            cur.clear ();
          }
        cur.push_back (x);
      }
    if (!cur.empty ())
      out.push_back (list (cur));
    return list (out);
  });
  prim (s, "List.flatten", "[['a]] -> ['a]", 1, [] (C &, V &a) {
    std::vector<Value> out;
    for (auto &xs : items (a[0]))
      for (auto &x : items (xs))
        out.push_back (x);
    return list (out);
  });
  prim (s, "List.index_of", "'a -> ['a] -> Option Int", 2, [] (C &c, V &a) {
    std::int64_t i = 0;
    for (auto &x : items (a[1]))
      {
        if (metaml::values_equal (x, a[0], c.span))
          return some (integer (i));
        ++i;
      }
    return none ();
  });
  prim (s, "List.transpose", "[['a]] -> [['a]]", 1, [] (C &, V &a) {
    std::vector<std::vector<Value>> rows;
    for (auto &r : items (a[0]))
      rows.push_back (items (r));
    std::vector<Value> out;
    if (rows.empty ())
      return list (out);
    std::size_t n = rows[0].size ();
    for (auto &r : rows)
      n = std::min (n, r.size ());
    for (std::size_t k = 0; k < n; ++k)
      {
        std::vector<Value> col;
        for (auto &r : rows)
          col.push_back (r[k]);
        out.push_back (list (col));
      }
    return list (out);
  });
  prim (s, "List.scan", "('b -> 'a -> 'b) -> 'b -> ['a] -> ['b]", 3, [] (C &c, V &a) {
    std::vector<Value> out{ a[1] };
    Value acc = a[1];
    for (auto &x : items (a[2]))
      {
        acc = c.apply (a[0], { acc, x });
        out.push_back (acc);
      }
    return list (out);
  });
  prim (s, "List.iterate", "Int -> ('a -> 'a) -> 'a -> ['a]", 3, [] (C &c, V &a) {
    std::vector<Value> out;
    Value x = a[2];
    for (std::int64_t k = 0; k < a[0].i; ++k)
      {
        out.push_back (x);
        if (k + 1 < a[0].i)
          x = c.apply (a[1], { x });
      }
    return list (out);
  });
  prim (s, "List.zip3", "['a] -> ['b] -> ['c] -> [('a, 'b, 'c)]", 3, [] (C &, V &a) {
    auto x = items (a[0]), y = items (a[1]), z = items (a[2]);
    std::vector<Value> out;
    for (std::size_t i = 0; i < x.size () && i < y.size () && i < z.size (); ++i)
      out.push_back (triple (x[i], y[i], z[i]));
    return list (out);
  });
  prim (s, "List.rotate", "Int -> ['a] -> ['a]", 2, [] (C &, V &a) {
    auto xs = items (a[1]);
    if (xs.empty ())
      return list (xs);
    std::int64_t n = static_cast<std::int64_t> (xs.size ());
    std::int64_t k = ((a[0].i % n) + n) % n;
    std::rotate (xs.begin (), xs.begin () + k, xs.end ());
    return list (xs);
  });
  prim (s, "List.shuffle", "Int -> ['a] -> ['a]", 2, [] (C &, V &a) {
    auto xs = items (a[1]);
    std::uint64_t st = static_cast<std::uint64_t> (a[0].i) ^ 0x9E3779B97F4A7C15ULL;
    if (st == 0)
      st = 1;
    auto next = [&st] ()
      {
        st ^= st >> 12;
        st ^= st << 25;
        st ^= st >> 27;
        return st * 2685821657736338717ULL;
      };
    for (std::size_t i = xs.size (); i > 1; --i)
      {
        std::size_t j = static_cast<std::size_t> (next () % i);
        std::swap (xs[i - 1], xs[j]);
      }
    return list (xs);
  });
  auto extreme = [] (bool want_max)
    {
      return [want_max] (C &c, V &a) -> Value {
        auto xs = items (a[1]);
        if (xs.empty ())
          return none ();
        Value best = xs[0];
        for (std::size_t i = 1; i < xs.size (); ++i)
          {
            auto r = c.apply (a[0], { xs[i], best }).i;
            if (want_max ? r > 0 : r < 0)
              best = xs[i];
          }
        return some (best);
      };
    };
  prim (s, "List.max_by", "('a -> 'a -> Int) -> ['a] -> Option 'a", 2, extreme (true));
  prim (s, "List.min_by", "('a -> 'a -> Int) -> ['a] -> Option 'a", 2, extreme (false));
}

} // namespace metalib
