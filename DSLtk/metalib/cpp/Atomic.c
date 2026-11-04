#pragma once
namespace metalib {
inline void install_Atomic(metaml::Session &s)
{
  using namespace support;
  if (s.find_primitive("Atomic.create")) return;
  s.declare("type AtomicInt\n", "<metalib Atomic>");
  prim(s, "Atomic.create", "Int -> AtomicInt", 1, [](C &, V &a) {
    auto h = std::make_shared<Handle<std::int64_t>>("AtomicInt");
    h->data = a[0].i; return wrap_handle(h);
  });
  prim(s, "Atomic.load", "AtomicInt -> Int", 1, [](C &c, V &a) {
    auto &h = handle<std::int64_t>(a[0], "AtomicInt", c);
    std::lock_guard<std::mutex> lock(h.mutex); return integer(h.data);
  });
  prim(s, "Atomic.store", "AtomicInt -> Int -> Unit", 2, [](C &c, V &a) {
    auto &h = handle<std::int64_t>(a[0], "AtomicInt", c);
    std::lock_guard<std::mutex> lock(h.mutex); h.data = a[1].i; return unit();
  });
  prim(s, "Atomic.exchange", "AtomicInt -> Int -> Int", 2, [](C &c, V &a) {
    auto &h = handle<std::int64_t>(a[0], "AtomicInt", c);
    std::lock_guard<std::mutex> lock(h.mutex); auto old = h.data; h.data = a[1].i; return integer(old);
  });
  prim(s, "Atomic.fetch_add", "AtomicInt -> Int -> Int", 2, [](C &c, V &a) {
    auto &h = handle<std::int64_t>(a[0], "AtomicInt", c);
    std::lock_guard<std::mutex> lock(h.mutex); auto old = h.data;
    h.data = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(old) + static_cast<std::uint64_t>(a[1].i));
    return integer(old);
  });
  prim(s, "Atomic.compare_exchange", "AtomicInt -> Int -> Int -> (Bool, Int)", 3, [](C &c, V &a) {
    auto &h = handle<std::int64_t>(a[0], "AtomicInt", c);
    std::lock_guard<std::mutex> lock(h.mutex); auto old = h.data;
    bool matched = old == a[1].i; if (matched) h.data = a[2].i;
    return pair(boolean(matched), integer(old));
  });
}
}
