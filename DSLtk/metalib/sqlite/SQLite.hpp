#pragma once
#include "../cpp/Support.h"
#include <sqlite3.h>
#include <climits>

namespace metalib {
namespace sqlite_detail {
struct Database {
  sqlite3 *db = nullptr;
  Database() = default;
  Database(const Database &) = delete;
  ~Database() { if (db) sqlite3_close_v2(db); }
};
inline support::Value query(support::C &c, support::V &args) {
  using namespace support;
  auto &h = handle<Database>(args[0], "SQLiteDB", c);
  std::lock_guard lock(h.mutex);
  if (!h.data.db) return err("database is closed");
  const auto &sql = S(args[1]);
  if (sql.size() > INT_MAX || sql.find('\0') != std::string::npos) return err("invalid SQL text");
  sqlite3_stmt *stmt = nullptr;
  const char *tail = nullptr;
  if (sqlite3_prepare_v2(h.data.db, sql.c_str(), static_cast<int>(sql.size()), &stmt, &tail) != SQLITE_OK)
    return err(sqlite3_errmsg(h.data.db));
  struct Guard { sqlite3_stmt *p; ~Guard() { sqlite3_finalize(p); } } guard{stmt};
  if (!stmt) return err("expected one SQL statement");
  while (tail && (*tail == ' ' || *tail == '\n' || *tail == '\r' || *tail == '\t')) ++tail;
  if (tail && *tail) return err("expected exactly one SQL statement");
  auto params = items(args[2]);
  if (params.size() != static_cast<std::size_t>(sqlite3_bind_parameter_count(stmt))) return err("parameter count mismatch");
  for (std::size_t i = 0; i < params.size(); ++i) {
    const auto &v = params[i];
    int rc = SQLITE_MISUSE, index = static_cast<int>(i + 1);
    if (is_ctor(v, "SqlNull")) rc = sqlite3_bind_null(stmt, index);
    else if (is_ctor(v, "SqlInt")) rc = sqlite3_bind_int64(stmt, index, arg(v, 0).i);
    else if (is_ctor(v, "SqlFloat")) rc = sqlite3_bind_double(stmt, index, arg(v, 0).f);
    else {
      const auto &bytes = S(arg(v, 0));
      if (bytes.size() > INT_MAX) return err("SQL value too large");
      rc = is_ctor(v, "SqlBlob")
        ? sqlite3_bind_blob(stmt, index, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT)
        : sqlite3_bind_text(stmt, index, bytes.data(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
    }
    if (rc != SQLITE_OK) return err(sqlite3_errmsg(h.data.db));
  }
  V rows;
  int rc;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    c.interp.check_cancelled(c.span);
    V row;
    for (int col = 0; col < sqlite3_column_count(stmt); ++col) {
      Value value;
      switch (sqlite3_column_type(stmt, col)) {
        case SQLITE_NULL: value = metaml::make_ctor("SqlNull", 0); break;
        case SQLITE_INTEGER: value = metaml::make_ctor("SqlInt", 1, {integer(sqlite3_column_int64(stmt, col))}); break;
        case SQLITE_FLOAT: value = metaml::make_ctor("SqlFloat", 2, {floating(sqlite3_column_double(stmt, col))}); break;
        default: {
          bool blob = sqlite3_column_type(stmt, col) == SQLITE_BLOB;
          auto p = blob ? sqlite3_column_blob(stmt, col) : sqlite3_column_text(stmt, col);
          auto n = sqlite3_column_bytes(stmt, col);
          value = metaml::make_ctor(blob ? "SqlBlob" : "SqlText", blob ? 4 : 3, {str(n ? std::string(static_cast<const char *>(static_cast<const void *>(p)), n) : "")});
        }
      }
      row.push_back(pair(str(sqlite3_column_name(stmt, col)), value));
    }
    rows.push_back(list(row));
  }
  if (rc != SQLITE_DONE) return err(sqlite3_errmsg(h.data.db));
  return ok(list(rows));
}
}
inline void install_SQLite(metaml::Session &s) {
  using namespace support;
  if (s.find_primitive("SQLite.connect")) return;
  s.declare("type SQLiteDB\ntype SqlValue := | SqlNull | SqlInt Int | SqlFloat Float | SqlText String | SqlBlob String\n", "<SQLite>");
  prim(s, "SQLite.connect", "String -> Result SQLiteDB String", 1, [](C &, V &a) {
    if (S(a[0]).find('\0') != std::string::npos) return err("invalid database path");
    auto h = std::make_shared<Handle<sqlite_detail::Database>>("SQLiteDB");
    if (sqlite3_open_v2(S(a[0]).c_str(), &h->data.db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
      return err(h->data.db ? sqlite3_errmsg(h->data.db) : "cannot allocate database");
    sqlite3_busy_timeout(h->data.db, 5000);
    return ok(wrap_handle(h));
  });
  prim(s, "SQLite.query", "SQLiteDB -> String -> [SqlValue] -> Result [[(String, SqlValue)]] String", 3, sqlite_detail::query);
  prim(s, "SQLite.close", "SQLiteDB -> Unit", 1, [](C &c, V &a) {
    auto &h = handle<sqlite_detail::Database>(a[0], "SQLiteDB", c);
    std::lock_guard lock(h.mutex);
    if (h.data.db) { sqlite3_close_v2(h.data.db); h.data.db = nullptr; }
    return unit();
  });
}
}
