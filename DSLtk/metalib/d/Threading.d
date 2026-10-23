/// Managed worker tasks, mirrored by metalib/cpp/Threading.h.
module metalib.d.Threading;
import metalib.d.Support;
import core.thread : Thread;
import core.sync.mutex : Mutex;
import core.sync.condition : Condition;
import core.atomic : atomicLoad, atomicStore;
import core.time : dur, MonoTime;

private class Task : ForeignObj
{
    Mutex mutex;
    Condition ready;
    shared bool cancelled;
    bool done;
    Value result;
    Interp interp;
    Thread worker;
    this() { super("Thread"); mutex = new Mutex; ready = new Condition(mutex); }
    void stop() { atomicStore(cancelled, true); mutex.lock(); ready.notifyAll(); mutex.unlock(); }
    void wait() { if (worker !is null && worker !is Thread.getThis()) worker.join(); }
}
private Task task(Value v, C c)
{
    auto t = cast(Task) v.obj;
    if (v.kind != VK.Foreign || t is null) c.error("M0404", "expected a Thread handle");
    return t;
}
void installThreading(Session s)
{
    if (s.find_primitive("Threading.spawn") !is null) return;
    s.declare("type Thread 'a\n", "<metalib Threading>");
    prim(s, "Threading.spawn", "(Unit -> 'a) -> Thread 'a", 1, (C c, V a) {
        auto t = new Task;
        t.interp = c.session.spawn_interp(true);
        t.interp.cancelled = () => atomicLoad(t.cancelled);
        c.session.on_close(() { t.stop(); }, () { t.wait(); });
        auto fn = a[0];
        t.worker = new Thread({
            Value result;
            try result = ok(t.interp.apply(fn, [unit()]));
            catch (MetaError e) { result = err(e.diag.headline()); }
            catch (Throwable e) { result = err("Threading.spawn: " ~ e.msg); }
            t.mutex.lock();
            t.result = result; t.done = true; t.ready.notifyAll();
            t.mutex.unlock();
        });
        t.worker.isDaemon = true;
        t.worker.start();
        return wrap_handle(t);
    });
    prim(s, "Threading.join", "Thread 'a -> Result 'a String", 1, (C c, V a) {
        auto t = task(a[0], c);
        if (t.worker is Thread.getThis()) return err("Threading.join: a task cannot join itself");
        t.mutex.lock(); scope(exit) t.mutex.unlock();
        while (!t.done) { c.interp.check_cancelled(c.span); t.ready.wait(dur!"msecs"(20)); }
        return t.result;
    });
    prim(s, "Threading.cancel", "Thread 'a -> Unit", 1, (C c, V a) { task(a[0], c).stop(); return unit(); });
    prim(s, "Threading.finished", "Thread 'a -> Bool", 1, (C c, V a) {
        auto t = task(a[0], c); t.mutex.lock(); scope(exit) t.mutex.unlock(); return boolean(t.done);
    });
    prim(s, "Threading.sleep", "Int -> Unit", 1, (C c, V a) {
        if (a[0].i < 0 || a[0].i > 86400000) c.error("M0404", "`Threading.sleep` expects 0..86400000 milliseconds");
        auto end = MonoTime.currTime + dur!"msecs"(a[0].i);
        while (MonoTime.currTime < end)
        {
            c.interp.check_cancelled(c.span);
            auto left = end - MonoTime.currTime;
            if (left > dur!"msecs"(20)) left = dur!"msecs"(20);
            if (left > dur!"msecs"(0)) Thread.sleep(left);
        }
        return unit();
    });
    prim(s, "Threading.yield", "Unit -> Unit", 1, (C c, V a) { Thread.yield(); return unit(); });
}
