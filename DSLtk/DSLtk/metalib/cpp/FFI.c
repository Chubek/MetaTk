// metalib/cpp/FFI.c — implementation of FFI.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// FFI.h and must not be compiled on its own. Mirror: metalib/d/FFI.d.
#pragma once

namespace metalib
{

namespace ffi_detail
{
using metaml::Value;

#if (defined(__x86_64__) || defined(__aarch64__)) && (defined(__unix__) || defined(__APPLE__))
constexpr bool available = true;
#else
constexpr bool available = false;
#endif

inline void
declare_type (metaml::Session &s)
{
  if (s.checker ().global->types.find ("FfiValue"))
    return;
  s.declare ("type FfiValue := | FInt Int | FFloat Float | FStr String | FVoid\n", "<metalib FFI>");
}

/// Opened libraries are cached for the life of the process.
inline void *
open_lib (const std::string &lib, std::string &why)
{
  static std::mutex m;
  static std::map<std::string, void *> cache;
  std::lock_guard<std::mutex> g (m);
  auto it = cache.find (lib);
  if (it != cache.end ())
    return it->second;
  void *h = ::dlopen (lib.empty () ? nullptr : lib.c_str (), RTLD_NOW | RTLD_LOCAL);
  if (!h)
    {
      const char *e = ::dlerror ();
      why = "cannot open library `" + lib + "`" + (e ? std::string (": ") + e : std::string ());
      return nullptr;
    }
  cache[lib] = h;
  return h;
}

/// Calls `fn` per the signature "R(A...)": R, A in i (C int), l (int64 /
/// pointer-sized), d (double), s (C string), v (void, return only). Up to six
/// integer-class and eight double arguments. Relies on the SysV x86-64 and
/// AAPCS64 conventions passing integer and floating arguments in separate
/// register files, so one fixed prototype serves every signature.
inline Value
call (metaml::PrimContext &c, void *fn, const std::string &sig, const std::vector<Value> &args)
{
  using namespace support;
  if (sig.size () < 3 || sig[1] != '(' || sig.back () != ')')
    return err ("FFI.call: malformed signature `" + sig + "` (expected e.g. `d(d)`)");
  char ret = sig[0];
  std::string ps = sig.substr (2, sig.size () - 3);
  if (std::string ("ildsv").find (ret) == std::string::npos)
    return err ("FFI.call: unknown return type `" + std::string (1, ret) + "`");
  if (ps.size () != args.size ())
    return err ("FFI.call: signature `" + sig + "` takes " + std::to_string (ps.size ())
                + " argument(s) but " + std::to_string (args.size ()) + " were given");
  long ints[6] = { 0, 0, 0, 0, 0, 0 };
  double dbls[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  std::size_t ni = 0, nd = 0;
  for (std::size_t k = 0; k < ps.size (); ++k)
    {
      char p = ps[k];
      auto a = metaml::agg_of (args[k]);
      const std::string &tag = a->tag;
      if (p == 'i' || p == 'l')
        {
          if (tag != "FInt")
            return err ("FFI.call: argument " + std::to_string (k + 1) + " must be FInt");
          if (ni >= 6)
            return err ("FFI.call: too many integer arguments (at most 6)");
          ints[ni++] = static_cast<long> (a->items[0].i);
        }
      else if (p == 'd')
        {
          if (tag != "FFloat")
            return err ("FFI.call: argument " + std::to_string (k + 1) + " must be FFloat");
          if (nd >= 8)
            return err ("FFI.call: too many float arguments (at most 8)");
          dbls[nd++] = a->items[0].f;
        }
      else if (p == 's')
        {
          if (tag != "FStr")
            return err ("FFI.call: argument " + std::to_string (k + 1) + " must be FStr");
          if (ni >= 6)
            return err ("FFI.call: too many integer arguments (at most 6)");
          ints[ni++] = reinterpret_cast<long> (metaml::str_of (a->items[0]).c_str ());
        }
      else
        return err ("FFI.call: unknown argument type `" + std::string (1, p) + "`");
    }
  (void)c;
  if (ret == 'd')
    {
      using F = double (*) (long, long, long, long, long, long, double, double, double, double,
                            double, double, double, double);
      double r = reinterpret_cast<F> (fn) (ints[0], ints[1], ints[2], ints[3], ints[4], ints[5],
                                           dbls[0], dbls[1], dbls[2], dbls[3], dbls[4], dbls[5],
                                           dbls[6], dbls[7]);
      return ok (metaml::make_ctor ("FFloat", 1, { floating (r) }));
    }
  using F = long (*) (long, long, long, long, long, long, double, double, double, double, double,
                      double, double, double);
  long r = reinterpret_cast<F> (fn) (ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], dbls[0],
                                     dbls[1], dbls[2], dbls[3], dbls[4], dbls[5], dbls[6], dbls[7]);
  switch (ret)
    {
    case 'i':
      return ok (metaml::make_ctor ("FInt", 0, { integer (static_cast<int> (r)) }));
    case 'l':
      return ok (metaml::make_ctor ("FInt", 0, { integer (static_cast<std::int64_t> (r)) }));
    case 's':
      {
        const char *p = reinterpret_cast<const char *> (r);
        return ok (metaml::make_ctor ("FStr", 2, { str (p ? p : "") }));
      }
    default:
      return ok (metaml::make_ctor ("FVoid", 3));
    }
}
} // namespace ffi_detail

inline void
install_FFI (metaml::Session &s)
{
  using namespace support;
  using namespace ffi_detail;
  declare_type (s);
  prim (s, "FFI.available", "Unit -> Bool", 1, [] (C &, V &) { return boolean (available); });
  prim (s, "FFI.symbol_exists", "String -> String -> Bool", 2, [] (C &, V &a) {
    if (!available)
      return boolean (false);
    std::string why;
    void *h = open_lib (S (a[0]), why);
    return boolean (h && ::dlsym (h, S (a[1]).c_str ()));
  });
  prim (s, "FFI.call", "String -> String -> String -> [FfiValue] -> Result FfiValue String", 4,
        [] (C &c, V &a) {
          if (!available)
            return err ("FFI.call: not supported on this platform");
          std::string why;
          void *h = open_lib (S (a[0]), why);
          if (!h)
            return err ("FFI.call: " + why);
          void *fn = ::dlsym (h, S (a[1]).c_str ());
          if (!fn)
            return err ("FFI.call: no symbol `" + S (a[1]) + "` in `" + S (a[0]) + "`");
          return call (c, fn, S (a[2]), items (a[3]));
        });
}

} // namespace metalib
