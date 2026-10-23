#pragma once
namespace metalib {
namespace threading_detail {
struct Task : metaml::ForeignObj
{
  std::mutex mutex, join_mutex;
  std::condition_variable ready;
  std::atomic<bool> cancelled{false};
  bool done = false;
  metaml::Value result;
  std::unique_ptr<metaml::Interp> interp;
  std::thread worker;
  Task () : ForeignObj ("Thread") {}
  void join ()
  {
    std::lock_guard<std::mutex> guard (join_mutex);
    if (worker.joinable () && worker.get_id () != std::this_thread::get_id ()) worker.join ();
  }
  ~Task () { join (); }
};
inline std::shared_ptr<Task> task (const metaml::Value &v, support::C &c)
{
  auto t = std::dynamic_pointer_cast<Task> (v.obj);
  if (v.kind != metaml::VK::Foreign || !t) c.error ("M0404", "expected a Thread handle");
  return t;
}
}
inline void install_Threading (metaml::Session &s)
{
  using namespace support;
  using namespace threading_detail;
  if (s.find_primitive ("Threading.spawn")) return;
  s.declare ("type Thread 'a\n", "<metalib Threading>");
  prim (s, "Threading.spawn", "(Unit -> 'a) -> Thread 'a", 1, [] (C &c, V &a) {
    auto t = std::make_shared<Task> ();
    t->interp = c.session.spawn_interp (true);
    std::weak_ptr<Task> weak = t;
    t->interp->cancelled = [weak] { auto p = weak.lock (); return !p || p->cancelled.load (); };
    c.session.on_close ([t] { t->cancelled.store (true); t->ready.notify_all (); }, [t] { t->join (); });
    auto fn = a[0];
    t->worker = std::thread ([t, fn] {
      Value result;
      try { result = ok (t->interp->apply (fn, { unit () })); }
      catch (const metaml::detail::MetaError &e) { result = err (e.diag.headline ()); }
      catch (const std::exception &e) { result = err (std::string ("Threading.spawn: ") + e.what ()); }
      catch (...) { result = err ("Threading.spawn: unknown host failure"); }
      {
        std::lock_guard<std::mutex> guard (t->mutex);
        t->result = std::move (result); t->done = true;
      }
      t->ready.notify_all ();
    });
    Value v; v.kind = metaml::VK::Foreign; v.obj = t; return v;
  });
  prim (s, "Threading.join", "Thread 'a -> Result 'a String", 1, [] (C &c, V &a) {
    auto t = task (a[0], c);
    if (t->worker.get_id () == std::this_thread::get_id ())
      return err ("Threading.join: a task cannot join itself");
    std::unique_lock<std::mutex> guard (t->mutex);
    while (!t->done) { c.interp.check_cancelled (c.span); t->ready.wait_for (guard, std::chrono::milliseconds (20)); }
    return t->result;
  });
  prim (s, "Threading.cancel", "Thread 'a -> Unit", 1, [] (C &c, V &a) {
    auto t = task (a[0], c); t->cancelled.store (true); t->ready.notify_all (); return unit ();
  });
  prim (s, "Threading.finished", "Thread 'a -> Bool", 1, [] (C &c, V &a) {
    auto t = task (a[0], c); std::lock_guard<std::mutex> guard (t->mutex); return boolean (t->done);
  });
  prim (s, "Threading.sleep", "Int -> Unit", 1, [] (C &c, V &a) {
    if (a[0].i < 0 || a[0].i > 86400000) c.error ("M0404", "`Threading.sleep` expects 0..86400000 milliseconds");
    auto end = std::chrono::steady_clock::now () + std::chrono::milliseconds (a[0].i);
    while (std::chrono::steady_clock::now () < end) {
      c.interp.check_cancelled (c.span);
      std::this_thread::sleep_until (std::min (end, std::chrono::steady_clock::now () + std::chrono::milliseconds (20)));
    }
    return unit ();
  });
  prim (s, "Threading.yield", "Unit -> Unit", 1, [] (C &, V &) { std::this_thread::yield (); return unit (); });
}
}
