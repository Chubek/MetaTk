#pragma once
namespace metalib {
namespace channels_detail {
struct Channel : metaml::ForeignObj
{
  std::mutex mutex;
  std::condition_variable ready;
  std::deque<metaml::Value> queue;
  std::size_t capacity;
  bool closed = false;
  explicit Channel (std::size_t n) : ForeignObj ("Channel"), capacity (n) {}
  void close () { std::lock_guard<std::mutex> g (mutex); closed = true; ready.notify_all (); }
};
inline std::shared_ptr<Channel> channel (const metaml::Value &v, support::C &c)
{
  auto h = std::dynamic_pointer_cast<Channel> (v.obj);
  if (v.kind != metaml::VK::Foreign || !h) c.error ("M0404", "expected a Channel handle");
  return h;
}
inline metaml::Value receive (support::C &c, const metaml::Value &v, std::int64_t timeout)
{
  auto h = channel (v, c);
  std::unique_lock<std::mutex> g (h->mutex);
  auto deadline = std::chrono::steady_clock::now () + std::chrono::milliseconds (timeout < 0 ? 0 : timeout);
  while (h->queue.empty () && !h->closed)
  {
    c.interp.check_cancelled (c.span);
    if (timeout >= 0 && std::chrono::steady_clock::now () >= deadline) return support::none ();
    h->ready.wait_for (g, std::chrono::milliseconds (timeout < 0 ? 20 : std::min<std::int64_t> (20, timeout)));
  }
  if (h->queue.empty ()) return support::none ();
  auto value = h->queue.front (); h->queue.pop_front (); h->ready.notify_all ();
  return support::some (value);
}
}
inline void install_Channels (metaml::Session &s)
{
  using namespace support;
  using namespace channels_detail;
  if (s.find_primitive ("Channels.create")) return;
  s.declare ("type Channel 'a\n", "<metalib Channels>");
  prim (s, "Channels.create", "Int -> Channel 'a", 1, [] (C &c, V &a) {
    if (a[0].i < 0 || a[0].i > 1000000) c.error ("M0404", "`Channels.create` expects a capacity in 0..1000000");
    auto h = std::make_shared<Channel> (static_cast<std::size_t> (a[0].i));
    c.session.on_close ([h] { h->close (); });
    Value v; v.kind = metaml::VK::Foreign; v.obj = h; return v;
  });
  prim (s, "Channels.send", "'a -> Channel 'a -> Result Unit String", 2, [] (C &c, V &a) {
    auto h = channel (a[1], c); std::unique_lock<std::mutex> g (h->mutex);
    while (!h->closed && h->capacity && h->queue.size () >= h->capacity) {
      c.interp.check_cancelled (c.span); h->ready.wait_for (g, std::chrono::milliseconds (20));
    }
    if (h->closed) return err ("Channels.send: channel is closed");
    h->queue.push_back (a[0]); h->ready.notify_all (); return ok (unit ());
  });
  prim (s, "Channels.recv", "Channel 'a -> Option 'a", 1, [] (C &c, V &a) { return receive (c, a[0], -1); });
  prim (s, "Channels.try_recv", "Channel 'a -> Option 'a", 1, [] (C &c, V &a) { return receive (c, a[0], 0); });
  prim (s, "Channels.recv_timeout", "Int -> Channel 'a -> Option 'a", 2, [] (C &c, V &a) {
    if (a[0].i < 0 || a[0].i > 86400000) c.error ("M0404", "`Channels.recv_timeout` expects 0..86400000 milliseconds");
    return receive (c, a[1], a[0].i);
  });
  prim (s, "Channels.try_send", "'a -> Channel 'a -> Bool", 2, [] (C &c, V &a) {
    auto h = channel (a[1], c); std::lock_guard<std::mutex> g (h->mutex);
    if (h->closed || (h->capacity && h->queue.size () >= h->capacity)) return boolean (false);
    h->queue.push_back (a[0]); h->ready.notify_all (); return boolean (true);
  });
  prim (s, "Channels.close", "Channel 'a -> Unit", 1, [] (C &c, V &a) { channel (a[0], c)->close (); return unit (); });
  prim (s, "Channels.size", "Channel 'a -> Int", 1, [] (C &c, V &a) {
    auto h = channel (a[0], c); std::lock_guard<std::mutex> g (h->mutex); return integer (h->queue.size ());
  });
  prim (s, "Channels.is_closed", "Channel 'a -> Bool", 1, [] (C &c, V &a) {
    auto h = channel (a[0], c); std::lock_guard<std::mutex> g (h->mutex); return boolean (h->closed);
  });
}
}
