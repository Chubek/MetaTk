#pragma once
namespace metalib {
namespace fiber_detail {
struct Fiber;
inline thread_local Fiber *current = nullptr;
struct Fiber : metaml::ForeignObj
{
  std::thread::id owner = std::this_thread::get_id ();
  ucontext_t context{}, caller{};
  std::vector<unsigned char> stack = std::vector<unsigned char> (1024 * 1024);
  std::unique_ptr<metaml::Interp> interp;
  metaml::Value fn, result;
  std::string state = "created", error;
  bool cancelled = false;
  Fiber () : ForeignObj ("Fiber") {}
  static void entry ()
  {
    auto *self = current;
    try { self->result = self->interp->apply (self->fn, { support::unit () }); self->state = "done"; }
    catch (const metaml::detail::MetaError &e) { self->error = e.diag.headline (); self->state = self->cancelled ? "cancelled" : "failed"; }
    catch (const std::exception &e) { self->error = std::string ("Fiber.resume: ") + e.what (); self->state = "failed"; }
    catch (...) { self->error = "Fiber.resume: unknown host failure"; self->state = "failed"; }
  }
  metaml::Value resume ()
  {
    using namespace support;
    if (owner != std::this_thread::get_id ()) return err ("Fiber.resume: fiber belongs to another thread");
    if (state == "running") return err ("Fiber.resume: fiber is already running");
    if (state == "done") return ok (some (result));
    if (state == "failed" || state == "cancelled") return err (error);
    Fiber *previous = current; current = this; state = "running";
    if (::swapcontext (&caller, &context) != 0) {
      current = previous; state = "failed"; error = "Fiber.resume: context switch failed"; return err (error);
    }
    current = previous;
    if (state == "suspended") return ok (none ());
    std::vector<unsigned char> ().swap (stack);
    return state == "done" ? ok (some (result)) : err (error);
  }
  void cancel ()
  {
    if (owner != std::this_thread::get_id ()) return;
    cancelled = true;
    if (state == "created") { state = "cancelled"; error = "error[M0410]: task cancelled"; std::vector<unsigned char> ().swap (stack); }
    else if (state == "suspended") (void)resume ();
  }
};
inline std::shared_ptr<Fiber> fiber (const metaml::Value &v, support::C &c)
{
  auto f = std::dynamic_pointer_cast<Fiber> (v.obj);
  if (v.kind != metaml::VK::Foreign || !f) c.error ("M0404", "expected a Fiber handle");
  if (f->owner != std::this_thread::get_id ()) c.error ("M0404", "fiber belongs to another thread");
  return f;
}
}
inline void install_Fiber (metaml::Session &s)
{
  using namespace support;
  using namespace fiber_detail;
  if (s.find_primitive ("Fiber.create")) return;
  s.declare ("type Fiber 'a\n", "<metalib Fiber>");
  prim (s, "Fiber.create", "(Unit -> 'a) -> Fiber 'a", 1, [] (C &c, V &a) {
    auto f = std::make_shared<Fiber> (); f->fn = a[0]; f->interp = c.session.spawn_interp ();
    std::weak_ptr<Fiber> weak = f;
    f->interp->cancelled = [weak] { auto p = weak.lock (); return !p || p->cancelled; };
    if (::getcontext (&f->context) != 0) c.error ("M0404", "cannot create fiber context");
    f->context.uc_stack.ss_sp = f->stack.data (); f->context.uc_stack.ss_size = f->stack.size ();
    f->context.uc_link = &f->caller; ::makecontext (&f->context, &Fiber::entry, 0);
    c.session.on_close ([f] { f->cancel (); });
    Value v; v.kind = metaml::VK::Foreign; v.obj = f; return v;
  });
  prim (s, "Fiber.resume", "Fiber 'a -> Result (Option 'a) String", 1, [] (C &c, V &a) { return fiber (a[0], c)->resume (); });
  prim (s, "Fiber.yield", "Unit -> Unit", 1, [] (C &c, V &) {
    auto *f = current;
    if (!f) c.error ("M0404", "`Fiber.yield` requires a running fiber");
    c.interp.check_cancelled (c.span); f->state = "suspended";
    if (::swapcontext (&f->context, &f->caller) != 0) c.error ("M0404", "fiber context switch failed");
    c.interp.check_cancelled (c.span); return unit ();
  });
  prim (s, "Fiber.cancel", "Fiber 'a -> Unit", 1, [] (C &c, V &a) { fiber (a[0], c)->cancel (); return unit (); });
  prim (s, "Fiber.status", "Fiber 'a -> String", 1, [] (C &c, V &a) { return str (fiber (a[0], c)->state); });
}
}
