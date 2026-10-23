// metalib/cpp/Table.c — implementation of Table.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Table.h and must not be compiled on its own. Mirror: metalib/d/Table.d.
#pragma once

namespace metalib
{


inline void
install_Table (metaml::Session &s)
{
  using namespace support;
  using Map = std::map<std::string, Value>;
  using H = Handle<Map>;
  s.declare ("type Table 'v\n", "<metalib Table>");
  prim (s, "Table.create", "Unit -> Table 'v", 1,
        [] (C &, V &) { return wrap_handle (std::make_shared<H> ("Table")); });
  prim (s, "Table.set", "String -> 'v -> Table 'v -> Unit", 3, [] (C &c, V &a) {
    auto &h = handle<Map> (a[2], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    h.data[S (a[0])] = a[1];
    return unit ();
  });
  prim (s, "Table.get", "String -> Table 'v -> Option 'v", 2, [] (C &c, V &a) {
    auto &h = handle<Map> (a[1], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    auto it = h.data.find (S (a[0]));
    if (it == h.data.end ())
      return none ();
    return some (it->second);
  });
  prim (s, "Table.has", "String -> Table 'v -> Bool", 2, [] (C &c, V &a) {
    auto &h = handle<Map> (a[1], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    return boolean (h.data.count (S (a[0])) > 0);
  });
  prim (s, "Table.remove", "String -> Table 'v -> Unit", 2, [] (C &c, V &a) {
    auto &h = handle<Map> (a[1], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    h.data.erase (S (a[0]));
    return unit ();
  });
  prim (s, "Table.size", "Table 'v -> Int", 1, [] (C &c, V &a) {
    auto &h = handle<Map> (a[0], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    return integer (static_cast<std::int64_t> (h.data.size ()));
  });
  prim (s, "Table.keys", "Table 'v -> [String]", 1, [] (C &c, V &a) {
    auto &h = handle<Map> (a[0], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    std::vector<Value> out;
    for (auto &kv : h.data)
      out.push_back (str (kv.first));
    return list (out);
  });
  prim (s, "Table.to_list", "Table 'v -> [(String, 'v)]", 1, [] (C &c, V &a) {
    auto &h = handle<Map> (a[0], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    std::vector<Value> out;
    for (auto &kv : h.data)
      out.push_back (pair (str (kv.first), kv.second));
    return list (out);
  });
  prim (s, "Table.of_list", "[(String, 'v)] -> Table 'v", 1, [] (C &, V &a) {
    auto h = std::make_shared<H> ("Table");
    for (auto &p : items (a[0]))
      h->data[S (fst (p))] = snd (p);
    return wrap_handle (h);
  });
  prim (s, "Table.clear", "Table 'v -> Unit", 1, [] (C &c, V &a) {
    auto &h = handle<Map> (a[0], "Table", c);
    std::lock_guard<std::mutex> g (h.mutex);
    h.data.clear ();
    return unit ();
  });
}

} // namespace metalib
