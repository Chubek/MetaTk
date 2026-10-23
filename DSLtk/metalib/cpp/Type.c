// metalib/cpp/Type.c — implementation of Type.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Type.h and must not be compiled on its own. Mirror: metalib/d/Type.d.
#pragma once

namespace metalib
{

namespace type_detail
{
using metaml::Value;
using metaml::TypeP;

struct Fail
{
  std::string why;
};

/// Resolves `src` in the global scope (at a fresh level). Names shared
/// through `m` denote the same variable.
inline TypeP
resolve (metaml::Checker &ck, const std::string &src, metaml::Checker::TVarMap &m)
{
  auto te = metaml::parse_type_expr (src, "<type>");
  if (te.is_err ())
    throw Fail{ frontend_detail::first_diag (te.unwrap_err ()) };
  try
    {
      return ck.resolve_type (te.unwrap (), ck.global, m);
    }
  catch (const metaml::detail::MetaError &e)
    {
      throw Fail{ frontend_detail::diag_text (e.diag) };
    }
}

inline void
make_rigid (const TypeP &t0, std::set<const metaml::TypeNode *> &seen)
{
  TypeP t = metaml::prune (t0);
  if (!t || !seen.insert (t.get ()).second)
    return;
  if (t->kind == metaml::TK::Var)
    {
      t->rigid = true;
      if (t->hint.empty ())
        t->hint = "'r" + std::to_string (seen.size ());
      return;
    }
  for (auto &x : t->args)
    make_rigid (x, seen);
  if (t->field)
    make_rigid (t->field, seen);
  if (t->rest)
    make_rigid (t->rest, seen);
}

/// Runs `body` with the unifier trail rolled back afterwards.
template <class F>
Value
scoped (metaml::Session &s, F body)
{
  auto &ck = s.checker ();
  auto snap = ck.u.snapshot ();
  int level = ck.level;
  ++ck.level;
  try
    {
      Value v = body (ck);
      ck.level = level;
      ck.u.rollback (snap);
      return v;
    }
  catch (const Fail &f)
    {
      ck.level = level;
      ck.u.rollback (snap);
      return support::err (f.why);
    }
}

inline bool
instance_of (metaml::Checker &ck, const std::string &a, const std::string &b)
{
  metaml::Checker::TVarMap ma, mb;
  auto ta = resolve (ck, a, ma);
  auto tb = resolve (ck, b, mb);
  std::set<const metaml::TypeNode *> seen;
  make_rigid (ta, seen);
  return ck.u.unify (tb, ta);
}

inline std::int64_t
arity (const TypeP &t0)
{
  TypeP t = metaml::prune (t0);
  std::int64_t n = 0;
  while (metaml::is_fun (t))
    {
      ++n;
      t = metaml::prune (t->args[1]);
    }
  return n;
}
} // namespace type_detail

inline void
install_Type (metaml::Session &s)
{
  using namespace support;
  using namespace type_detail;
  prim (s, "Type.normalize", "String -> Result String String", 1,
        [] (C &c, V &a) { return frontend_detail::format_type (c.session, S (a[0])); });
  prim (s, "Type.unify", "String -> String -> Result String String", 2, [] (C &c, V &a) {
    return scoped (c.session, [&] (metaml::Checker &ck) {
      metaml::Checker::TVarMap m;
      auto x = resolve (ck, S (a[0]), m);
      auto y = resolve (ck, S (a[1]), m);
      if (!ck.u.unify (x, y))
        return err ("Type.unify: `" + S (a[0]) + "` and `" + S (a[1]) + "` do not unify"
                    + (ck.u.reason.empty () ? std::string () : ": " + ck.u.reason));
      ck.generalize (x, ck.level - 1);
      return ok (str (metaml::show_type (x)));
    });
  });
  prim (s, "Type.instance_of", "String -> String -> Bool", 2, [] (C &c, V &a) {
    auto r = scoped (c.session, [&] (metaml::Checker &ck) {
      return boolean (instance_of (ck, S (a[0]), S (a[1])));
    });
    return r.kind == metaml::VK::Bool ? r : boolean (false);
  });
  prim (s, "Type.equal", "String -> String -> Bool", 2, [] (C &c, V &a) {
    auto one = scoped (c.session, [&] (metaml::Checker &ck) {
      return boolean (instance_of (ck, S (a[0]), S (a[1])));
    });
    auto two = scoped (c.session, [&] (metaml::Checker &ck) {
      return boolean (instance_of (ck, S (a[1]), S (a[0])));
    });
    return boolean (one.kind == metaml::VK::Bool && two.kind == metaml::VK::Bool && one.i
                    && two.i);
  });
  prim (s, "Type.arity", "String -> Result Int String", 1, [] (C &c, V &a) {
    return scoped (c.session, [&] (metaml::Checker &ck) {
      metaml::Checker::TVarMap m;
      return ok (integer (arity (resolve (ck, S (a[0]), m))));
    });
  });
}

} // namespace metalib
