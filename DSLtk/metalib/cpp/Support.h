/**
 * @file metalib/cpp/Support.h
 * @brief Shared helpers for the C++ metalib host modules.
 *
 * Every host module is header-only: `X.h` documents the MetaML-facing
 * primitives and includes `X.c`, which holds the C++ implementation (it is
 * C++ despite the extension and is never compiled on its own). The D
 * dialect mirrors each module in `metalib/d/X.d` with identical primitive
 * names, types and behaviour (AGENTS.md §18.2).
 */
#pragma once

#include "../../MetaML.hpp"

#include <condition_variable>
#include <deque>
#include <thread>

namespace metalib
{
namespace support
{

using metaml::Value;
using metaml::VK;
using V = std::vector<Value>;
using C = metaml::PrimContext;

inline const std::string &
S (const Value &v)
{
  return metaml::str_of (v);
}
inline Value
str (std::string s)
{
  return metaml::make_str (std::move (s));
}
inline Value
integer (std::int64_t i)
{
  return Value::integer (i);
}
inline Value
floating (double d)
{
  return Value::floating (d);
}
inline Value
boolean (bool b)
{
  return Value::boolean (b);
}
inline Value
unit ()
{
  return Value::unit ();
}
inline Value
some (Value v)
{
  return metaml::make_option (std::move (v));
}
inline Value
none ()
{
  return metaml::make_option (std::nullopt);
}
inline Value
ok (Value v)
{
  return metaml::make_ok (std::move (v));
}
inline Value
err (std::string msg)
{
  return metaml::make_err (metaml::make_str (std::move (msg)));
}
inline Value
list (const std::vector<Value> &items)
{
  return metaml::make_list (items);
}
inline std::vector<Value>
items (const Value &v)
{
  return metaml::list_items (v);
}
inline Value
pair (Value a, Value b)
{
  return metaml::make_tuple ({ std::move (a), std::move (b) });
}
inline Value
triple (Value a, Value b, Value c)
{
  return metaml::make_tuple ({ std::move (a), std::move (b), std::move (c) });
}
inline const Value &
fst (const Value &t)
{
  return metaml::agg_of (t)->items[0];
}
inline const Value &
snd (const Value &t)
{
  return metaml::agg_of (t)->items[1];
}
inline Value
record (std::vector<std::pair<std::string, Value>> fields)
{
  return metaml::make_record (std::move (fields));
}
inline Value
field (const Value &r, const std::string &label)
{
  auto f = metaml::record_field (r, label);
  return f ? *f : Value::unit ();
}
inline bool
is_ctor (const Value &v, const char *name)
{
  return v.kind == VK::Ctor && metaml::agg_of (v)->tag == name;
}
inline const Value &
arg (const Value &ctor, std::size_t i)
{
  return metaml::agg_of (ctor)->items[i];
}
inline Value
strings (const std::vector<std::string> &xs)
{
  std::vector<Value> out;
  for (auto &x : xs)
    out.push_back (str (x));
  return list (out);
}

/// Registers a sandboxed library primitive.
inline void
prim (metaml::Session &s, const std::string &name, const std::string &type, int arity,
      metaml::PrimFn fn)
{
  s.install_primitive (name, type, arity, std::move (fn));
}

/// Host handle holding a T (thread-safe access via `mutex`).
template <class T> struct Handle : metaml::ForeignObj
{
  T data{};
  std::mutex mutex{};
  explicit Handle (std::string type) : metaml::ForeignObj (std::move (type)) {}
};

template <class T>
Value
wrap_handle (const std::shared_ptr<Handle<T>> &h)
{
  Value v;
  v.kind = VK::Foreign;
  v.obj = h;
  return v;
}

template <class T>
Handle<T> &
handle (const Value &v, const char *type, C &ctx)
{
  auto h = dynamic_cast<Handle<T> *> (v.obj.get ());
  if (v.kind != VK::Foreign || !h || h->type_name != type)
    ctx.error ("M0404", std::string ("expected a ") + type + " handle");
  return *h;
}

/// Runs `body` on a thread with a large stack (pthread on POSIX).
inline std::thread
big_thread (std::function<void ()> body)
{
  return std::thread (std::move (body));
}

} // namespace support
} // namespace metalib
