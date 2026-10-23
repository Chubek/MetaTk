/// Stackful cooperative fibers (D runtime), mirrored by metalib/cpp/Fiber.h.
module metalib.d.Fiber;
import metalib.d.Support;
import core.thread : NativeFiber = Fiber, Thread;

private Fiber current;
private class Fiber : ForeignObj
{
    Thread owner;
    NativeFiber context;
    Interp interp;
    Value fn, result;
    string state = "created", error;
    bool cancelled;
    this(Value functionValue, Interp interpreter)
    {
        super("Fiber"); owner = Thread.getThis(); fn = functionValue; interp = interpreter;
        interp.cancelled = () => cancelled;
        context = new NativeFiber({
            try { result = interp.apply(fn, [unit()]); state = "done"; }
            catch (MetaError e) { error = e.diag.headline(); state = cancelled ? "cancelled" : "failed"; }
            catch (Throwable e) { error = "Fiber.resume: " ~ e.msg; state = "failed"; }
        }, 1024 * 1024);
    }
    Value resume()
    {
        if (owner !is Thread.getThis()) return err("Fiber.resume: fiber belongs to another thread");
        if (state == "running") return err("Fiber.resume: fiber is already running");
        if (state == "done") return ok(some(result));
        if (state == "failed" || state == "cancelled") return err(error);
        auto previous = current; current = this; scope(exit) current = previous;
        state = "running"; context.call();
        return state == "suspended" ? ok(none()) : state == "done" ? ok(some(result)) : err(error);
    }
    void cancel()
    {
        if (owner !is Thread.getThis()) return;
        cancelled = true;
        if (state == "created") { state = "cancelled"; error = "error[M0410]: task cancelled"; }
        else if (state == "suspended") resume();
    }
}
private Fiber fiber(Value v, C c)
{
    auto f = cast(Fiber) v.obj;
    if (v.kind != VK.Foreign || f is null) c.error("M0404", "expected a Fiber handle");
    if (f.owner !is Thread.getThis()) c.error("M0404", "fiber belongs to another thread");
    return f;
}
void installFiber(Session s)
{
    if (s.find_primitive("Fiber.create") !is null) return;
    s.declare("type Fiber 'a\n", "<metalib Fiber>");
    prim(s, "Fiber.create", "(Unit -> 'a) -> Fiber 'a", 1, (C c, V a) {
        auto f = new Fiber(a[0], c.session.spawn_interp());
        c.session.on_close(() { f.cancel(); }); return wrap_handle(f);
    });
    prim(s, "Fiber.resume", "Fiber 'a -> Result (Option 'a) String", 1, (C c, V a) => fiber(a[0], c).resume());
    prim(s, "Fiber.yield", "Unit -> Unit", 1, (C c, V a) {
        if (current is null) c.error("M0404", "`Fiber.yield` requires a running fiber");
        c.interp.check_cancelled(c.span); current.state = "suspended";
        NativeFiber.yield(); c.interp.check_cancelled(c.span); return unit();
    });
    prim(s, "Fiber.cancel", "Fiber 'a -> Unit", 1, (C c, V a) { fiber(a[0], c).cancel(); return unit(); });
    prim(s, "Fiber.status", "Fiber 'a -> String", 1, (C c, V a) => str(fiber(a[0], c).state));
}
