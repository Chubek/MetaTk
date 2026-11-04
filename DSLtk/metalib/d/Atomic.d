/// Linearizable signed 64-bit cells; mirrors cpp/Atomic.h.
module metalib.d.Atomic;
import metalib.d.Support;

void installAtomic(Session s)
{
    if (s.find_primitive("Atomic.create") !is null) return;
    s.declare("type AtomicInt\n", "<metalib Atomic>");
    prim(s, "Atomic.create", "Int -> AtomicInt", 1, (C c, V a) {
        auto h = new Handle!long("AtomicInt"); h.data = a[0].i; return wrap_handle(h);
    });
    prim(s, "Atomic.load", "AtomicInt -> Int", 1, (C c, V a) {
        auto h = handle!long(a[0], "AtomicInt", c);
        synchronized(h.mutex) { return integer(h.data); }
    });
    prim(s, "Atomic.store", "AtomicInt -> Int -> Unit", 2, (C c, V a) {
        auto h = handle!long(a[0], "AtomicInt", c);
        synchronized(h.mutex) { h.data = a[1].i; return unit(); }
    });
    prim(s, "Atomic.exchange", "AtomicInt -> Int -> Int", 2, (C c, V a) {
        auto h = handle!long(a[0], "AtomicInt", c);
        synchronized(h.mutex) { auto old = h.data; h.data = a[1].i; return integer(old); }
    });
    prim(s, "Atomic.fetch_add", "AtomicInt -> Int -> Int", 2, (C c, V a) {
        auto h = handle!long(a[0], "AtomicInt", c);
        synchronized(h.mutex) {
            auto old = h.data; h.data = cast(long)(cast(ulong)old + cast(ulong)a[1].i); return integer(old);
        }
    });
    prim(s, "Atomic.compare_exchange", "AtomicInt -> Int -> Int -> (Bool, Int)", 3, (C c, V a) {
        auto h = handle!long(a[0], "AtomicInt", c);
        synchronized(h.mutex) {
            auto old = h.data; bool matched = old == a[1].i;
            if (matched) h.data = a[2].i;
            return pair(boolean(matched), integer(old));
        }
    });
}
