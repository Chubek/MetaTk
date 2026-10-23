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
constexpr bool supported = true;
#else
constexpr bool supported = false;
#endif

// Use libffi rather than calling a symbol through an incompatible prototype.
// These opaque ABI buffers and exported type descriptors avoid a build-time
// dependency on libffi headers; the supported LP64 ABIs are selected explicitly.
struct Api
{
  void *library = nullptr;
  using Prep = int (*) (void *, unsigned, unsigned, void *, void **);
  using Call = void (*) (void *, void (*) (), void *, void **);
  Prep prep = nullptr;
  Call invoke = nullptr;
  void *i = nullptr, *l = nullptr, *d = nullptr, *s = nullptr, *v = nullptr;
  Api ()
  {
    if (!supported) return;
    for (auto name : { "libffi.so.8", "libffi.so", "libffi.dylib" })
      if ((library = ::dlopen (name, RTLD_NOW | RTLD_LOCAL))) break;
    if (!library) return;
    prep = reinterpret_cast<Prep> (::dlsym (library, "ffi_prep_cif"));
    invoke = reinterpret_cast<Call> (::dlsym (library, "ffi_call"));
    i = ::dlsym (library, "ffi_type_sint32"); l = ::dlsym (library, "ffi_type_sint64");
    d = ::dlsym (library, "ffi_type_double"); s = ::dlsym (library, "ffi_type_pointer");
    v = ::dlsym (library, "ffi_type_void");
  }
  bool ready () const { return prep && invoke && i && l && d && s && v; }
  void *type (char ch) const
  { return ch == 'i' ? i : ch == 'l' ? l : ch == 'd' ? d : ch == 's' ? s : v; }
};
inline Api &api () { static Api a; return a; }
inline bool available () { return api ().ready (); }

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
/// integer-class and eight double arguments. libffi performs ABI marshalling.
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
  struct Arg { int i = 0; std::int64_t l = 0; double d = 0; const char *s = nullptr; };
  std::vector<Arg> storage (args.size ());
  std::vector<void *> types, values;
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
          ++ni;
          storage[k].i = static_cast<int> (a->items[0].i);
          storage[k].l = a->items[0].i;
          values.push_back (p == 'i' ? static_cast<void *> (&storage[k].i) : &storage[k].l);
        }
      else if (p == 'd')
        {
          if (tag != "FFloat")
            return err ("FFI.call: argument " + std::to_string (k + 1) + " must be FFloat");
          if (nd >= 8)
            return err ("FFI.call: too many float arguments (at most 8)");
          ++nd;
          storage[k].d = a->items[0].f;
          values.push_back (&storage[k].d);
        }
      else if (p == 's')
        {
          if (tag != "FStr")
            return err ("FFI.call: argument " + std::to_string (k + 1) + " must be FStr");
          if (ni >= 6)
            return err ("FFI.call: too many integer arguments (at most 6)");
          ++ni;
          storage[k].s = metaml::str_of (a->items[0]).c_str ();
          values.push_back (&storage[k].s);
        }
      else
        return err ("FFI.call: unknown argument type `" + std::string (1, p) + "`");
      types.push_back (api ().type (p));
    }
  (void)c;
  alignas (16) unsigned char cif[256]{};
#if defined(__x86_64__) && !defined(__APPLE__)
  constexpr unsigned abi = 2; // FFI_UNIX64
#else
  constexpr unsigned abi = 1; // FFI_SYSV (AAPCS64)
#endif
  if (api ().prep (cif, abi, static_cast<unsigned> (types.size ()), api ().type (ret), types.data ()) != 0)
    return err ("FFI.call: cannot prepare the native call");
  union { std::uint64_t bits; double number; const char *text; } result{};
  api ().invoke (cif, reinterpret_cast<void (*) ()> (fn), &result, values.data ());
  auto r = static_cast<std::int64_t> (result.bits);
  if (ret == 'd') return ok (metaml::make_ctor ("FFloat", 1, { floating (result.number) }));
  switch (ret)
    {
    case 'i':
      return ok (metaml::make_ctor ("FInt", 0, { integer (static_cast<int> (r)) }));
    case 'l':
      return ok (metaml::make_ctor ("FInt", 0, { integer (static_cast<std::int64_t> (r)) }));
    case 's':
      {
        const char *p = result.text;
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
  prim (s, "FFI.available", "Unit -> Bool", 1, [] (C &, V &) { return boolean (available ()); });
  prim (s, "FFI.symbol_exists", "String -> String -> Bool", 2, [] (C &, V &a) {
    if (!available ())
      return boolean (false);
    std::string why;
    void *h = open_lib (S (a[0]), why);
    return boolean (h && ::dlsym (h, S (a[1]).c_str ()));
  });
  prim (s, "FFI.call", "String -> String -> String -> [FfiValue] -> Result FfiValue String", 4,
        [] (C &c, V &a) {
          if (!available ())
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
