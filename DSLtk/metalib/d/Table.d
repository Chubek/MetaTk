/**
 * metalib/d/Table.d — MetaML host module `Table` (D dialect): mutable string-keyed hash tables.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Table.h):
 *   Table.create, set, get, has, remove, size, keys, to_list, of_list, clear
 */
module metalib.d.Table;

import MetaML;
import metalib.d.Support;


private alias Map = Value[string];
private alias H = Handle!Map;

void installTable(Session s)
{
    s.declare("type Table 'v\n", "<metalib Table>");
    prim(s, "Table.create", "Unit -> Table 'v", 1, (C c, V a) => wrap_handle(new H("Table")));
    prim(s, "Table.set", "String -> 'v -> Table 'v -> Unit", 3, delegate Value(C c, V a) {
        auto h = handle!Map(a[2], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        h.data[S(a[0])] = a[1];
        return unit();
    });
    prim(s, "Table.get", "String -> Table 'v -> Option 'v", 2, delegate Value(C c, V a) {
        auto h = handle!Map(a[1], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        if (auto p = S(a[0]) in h.data) return some(*p);
        return none();
    });
    prim(s, "Table.has", "String -> Table 'v -> Bool", 2, delegate Value(C c, V a) {
        auto h = handle!Map(a[1], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        return boolean((S(a[0]) in h.data) !is null);
    });
    prim(s, "Table.remove", "String -> Table 'v -> Unit", 2, delegate Value(C c, V a) {
        auto h = handle!Map(a[1], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        h.data.remove(S(a[0]));
        return unit();
    });
    prim(s, "Table.size", "Table 'v -> Int", 1, delegate Value(C c, V a) {
        auto h = handle!Map(a[0], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        return integer(cast(long) h.data.length);
    });
    prim(s, "Table.keys", "Table 'v -> [String]", 1, delegate Value(C c, V a) {
        auto h = handle!Map(a[0], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        return strings(sortedKeys(h.data));
    });
    prim(s, "Table.to_list", "Table 'v -> [(String, 'v)]", 1, delegate Value(C c, V a) {
        auto h = handle!Map(a[0], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        Value[] result;
        foreach (k; sortedKeys(h.data)) result ~= pair(str(k), h.data[k]);
        return list(result);
    });
    prim(s, "Table.of_list", "[(String, 'v)] -> Table 'v", 1, delegate Value(C c, V a) {
        auto h = new H("Table");
        foreach (p; items(a[0])) h.data[S(fst(p))] = snd(p);
        return wrap_handle(h);
    });
    prim(s, "Table.clear", "Table 'v -> Unit", 1, delegate Value(C c, V a) {
        auto h = handle!Map(a[0], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        h.data = null;
        return unit();
    });
    prim(s, "Table.clone", "Table 'v -> Table 'v", 1, delegate Value(C c, V a) {
        auto h = handle!Map(a[0], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        auto copy = new H("Table");
        copy.data = h.data.dup;
        return wrap_handle(copy);
    });
    prim(s, "Table.values", "Table 'v -> ['v]", 1, delegate Value(C c, V a) {
        auto h = handle!Map(a[0], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        Value[] result;
        foreach (k; sortedKeys(h.data)) result ~= h.data[k];
        return list(result);
    });
    prim(s, "Table.set_if_absent", "String -> 'v -> Table 'v -> Bool", 3, delegate Value(C c, V a) {
        auto h = handle!Map(a[2], "Table", c);
        h.mutex.lock();
        scope (exit) h.mutex.unlock();
        auto key = S(a[0]);
        if (key in h.data) return boolean(false);
        h.data[key] = a[1];
        return boolean(true);
    });
}
