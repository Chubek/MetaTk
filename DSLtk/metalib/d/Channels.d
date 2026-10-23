/// Closeable FIFO channels, mirrored by metalib/cpp/Channels.h.
module metalib.d.Channels;
import metalib.d.Support;
import core.sync.mutex : Mutex;
import core.sync.condition : Condition;
import core.time : MonoTime, dur;

private class Channel : ForeignObj
{
    Mutex mutex;
    Condition ready;
    Value[] queue;
    size_t head, capacity;
    bool closed;
    this(size_t n) { super("Channel"); capacity = n; mutex = new Mutex; ready = new Condition(mutex); }
    void close() { mutex.lock(); closed = true; ready.notifyAll(); mutex.unlock(); }
    size_t size() { return queue.length - head; }
    Value pop()
    {
        auto value = queue[head]; queue[head++] = Value.unit();
        if (head == queue.length) { queue = null; head = 0; }
        else if (head > 1024 && head > queue.length / 2) { queue = queue[head .. $].dup; head = 0; }
        ready.notifyAll(); return value;
    }
}
private Channel channel(Value v, C c)
{
    auto h = cast(Channel) v.obj;
    if (v.kind != VK.Foreign || h is null) c.error("M0404", "expected a Channel handle");
    return h;
}
private Value receive(C c, Value v, long timeout)
{
    auto h = channel(v, c);
    h.mutex.lock(); scope(exit) h.mutex.unlock();
    auto end = MonoTime.currTime + dur!"msecs"(timeout < 0 ? 0 : timeout);
    while (!h.size() && !h.closed)
    {
        c.interp.check_cancelled(c.span);
        if (timeout >= 0 && MonoTime.currTime >= end) return none();
        h.ready.wait(dur!"msecs"(timeout < 0 ? 20 : timeout < 20 ? timeout : 20));
    }
    return h.size() ? some(h.pop()) : none();
}
void installChannels(Session s)
{
    if (s.find_primitive("Channels.create") !is null) return;
    s.declare("type Channel 'a\n", "<metalib Channels>");
    prim(s, "Channels.create", "Int -> Channel 'a", 1, (C c, V a) {
        if (a[0].i < 0 || a[0].i > 1000000) c.error("M0404", "`Channels.create` expects a capacity in 0..1000000");
        auto h = new Channel(cast(size_t) a[0].i);
        c.session.on_close(() { h.close(); }); return wrap_handle(h);
    });
    prim(s, "Channels.send", "'a -> Channel 'a -> Result Unit String", 2, (C c, V a) {
        auto h = channel(a[1], c); h.mutex.lock(); scope(exit) h.mutex.unlock();
        while (!h.closed && h.capacity && h.size() >= h.capacity)
        { c.interp.check_cancelled(c.span); h.ready.wait(dur!"msecs"(20)); }
        if (h.closed) return err("Channels.send: channel is closed");
        h.queue ~= a[0]; h.ready.notifyAll(); return ok(unit());
    });
    prim(s, "Channels.recv", "Channel 'a -> Option 'a", 1, (C c, V a) => receive(c, a[0], -1));
    prim(s, "Channels.try_recv", "Channel 'a -> Option 'a", 1, (C c, V a) => receive(c, a[0], 0));
    prim(s, "Channels.recv_timeout", "Int -> Channel 'a -> Option 'a", 2, (C c, V a) {
        if (a[0].i < 0 || a[0].i > 86400000) c.error("M0404", "`Channels.recv_timeout` expects 0..86400000 milliseconds");
        return receive(c, a[1], a[0].i);
    });
    prim(s, "Channels.try_send", "'a -> Channel 'a -> Bool", 2, (C c, V a) {
        auto h = channel(a[1], c); h.mutex.lock(); scope(exit) h.mutex.unlock();
        if (h.closed || (h.capacity && h.size() >= h.capacity)) return boolean(false);
        h.queue ~= a[0]; h.ready.notifyAll(); return boolean(true);
    });
    prim(s, "Channels.close", "Channel 'a -> Unit", 1, (C c, V a) { channel(a[0], c).close(); return unit(); });
    prim(s, "Channels.size", "Channel 'a -> Int", 1, (C c, V a) {
        auto h = channel(a[0], c); h.mutex.lock(); scope(exit) h.mutex.unlock(); return integer(h.size());
    });
    prim(s, "Channels.is_closed", "Channel 'a -> Bool", 1, (C c, V a) {
        auto h = channel(a[0], c); h.mutex.lock(); scope(exit) h.mutex.unlock(); return boolean(h.closed);
    });
}
