#include "metalib/cpp/Metalib.h"
#include "metalib/http/HTTP.hpp"
#include "metalib/uri/URI.hpp"
#include "metalib/ffi/FFI.hpp"
#include "metalib/parsec/Parsec.hpp"
#include "metalib/memory/Memory.hpp"
#include "metalib/shell/Shell.hpp"
#include "metalib/jsonrpc/JSONRPC.hpp"
#include "metalib/msgpack/MsgPack.hpp"
#include "metalib/sysipc/IPC.hpp"
#ifdef METAML_TEST_SERVICES
#include "metalib/sqlite/SQLite.hpp"
#include "metalib/nng/NNG.hpp"
#include "metalib/webserver/Webserver.hpp"
#endif
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  metaml::Session s;
  metalib::install_all(s);
  metalib::install_HTTP(s); metalib::install_FFI(s); metalib::install_Shell(s);
  metalib::install_MsgPack(s); metalib::install_IPC(s);
#ifdef METAML_TEST_SERVICES
  metalib::install_SQLite(s); metalib::install_NNG(s); metalib::install_Webserver(s);
#endif
  s.allow_all_primitives();
  if (s.load_stdlib(argv[1]).is_err()) throw std::runtime_error("prelude failed");
  auto eval = [&](const std::string &source) {
    auto result = s.eval(source);
    if (result.is_err()) {
      std::cerr << source << '\n';
      for (const auto &d : result.unwrap_err()) std::cerr << d.message << '\n';
      throw std::runtime_error("evaluation failed");
    }
    return result;
  };
  auto check = [&](const std::string &source) {
    if (metaml::to_string(eval(source)) != "true") throw std::runtime_error(source);
  };
  for (auto name : {"http", "uri", "ffi", "memory", "parsec", "shell", "sequence", "array", "stream", "getopt", "db", "nosqldb", "transpile", "weblets", "jsonrpc", "msgpack", "sysipc"})
    eval(std::string("import \"") + name + "\"");
  check("Array.set 1 9 [1,2,3] == Some [1,9,3]");
  check("Array.slice 2 2 [1,2,3] == None");
  check("Sequence.unfold (fn n -> if n == 3 then None else Some (n, n + 1)) 0 == [0,1,2]");
  check("Stream.take 3 (Stream.map (fn x -> x * 2) (Stream.unfold (fn n -> Some (n, n + 1)) 1)) == [2,4,6]");
  check("Getopt.parse [(\"name\", true), (\"verbose\", false)] [\"--name=x\", \"--verbose\", \"--\", \"--file\"] == Ok ([(\"name\", Some \"x\"), (\"verbose\", None)], [\"--file\"])");
  check("match Getopt.parse [(\"name\", true)] [\"--name\"] with | Err _ -> true | _ -> false");
  check("Db.get \"x\" (Db.put \"x\" 2 (Db.put \"x\" 1 [])) == Some 2");
  check("match NoSQL.insert 1 \"b\" [(1, \"a\")] with | Err _ -> true | _ -> false");
  check("Transpile.pipeline [Transpile.replace \"a\" \"b\", Transpile.replace \"b\" \"c\"] \"a\" == Ok \"c\"");
  check("Weblets.text \"<&\\\"\" == \"&lt;&amp;&quot;\"");
  check("Shell.quote \"a'b\" == \"'a'\\\\''b'\"");
  check("Shell.command (\"printf %s \" ^ Shell.quote \"a'b\") == Ok (0, \"a'b\")");
  check("Parser.run (Parser.many (Parser.char \"a\")) \"aaa\" == Ok [\"a\",\"a\",\"a\"]");
  check("JsonRPC.dispatch (fn method params -> Ok params) (JsonRPC.notification \"echo\" (JArr [])) == None");
  check("JsonRPC.dispatch (fn method params -> Ok params) (JsonRPC.request (JNum 1.0) \"echo\" (JArr [])) == Some (JsonRPC.success (JNum 1.0) (JArr []))");
  check("match JsonRPC.handle (fn method params -> Ok params) \"{\" with | Some _ -> true | _ -> false");
  check("let x := MpMap [(MpString \"k\", MpArray [MpInt (~9223372036854775808), MpBinary \"a\", MpExt (~1) \"z\"])] in match MsgPack.pack x with | Ok bytes -> MsgPack.unpack bytes == Ok x | _ -> false");
  check("MsgPack.unpack (String.of_codes [147, 1, 255, 195]) == Ok (MpArray [MpInt 1, MpInt (~1), MpBool true])");
  check("match MsgPack.unpack (String.of_codes [219, 0, 0, 0, 8, 97]) with | Err _ -> true | _ -> false");
  check("match MsgPack.unpack (String.of_codes [192,192]) with | Err _ -> true | _ -> false");
  check("MsgPack.unpack (String.of_codes [202,63,192,0,0]) == Ok (MpFloat 1.5)");
  check("MsgPack.unpack (String.of_codes [212,255,42]) == Ok (MpExt (~1) (String.of_codes [42]))");
  check("match MsgPack.unpack (String.of_codes [207,255,255,255,255,255,255,255,255]) with | Err _ -> true | _ -> false");
  check("MsgPack.pack (MpBool true) == Ok (String.of_codes [195])");
  eval("val endpoints := match SysIPC.pair () with | Ok pair -> pair | Err error -> Core.error error");
  check("SysIPC.send (Core.fst endpoints) \"message\" == Ok ()");
  check("SysIPC.recv (Core.snd endpoints) == Ok \"message\"");
  eval("SysIPC.close (Core.fst endpoints)"); eval("SysIPC.close (Core.snd endpoints)");
#ifdef METAML_TEST_SERVICES
  for (auto name : {"sqlite", "sqldb", "nng", "webserver"}) eval(std::string("import \"") + name + "\"");
  eval("val db := match SQL.connect \":memory:\" with | Ok db -> db | Err e -> Core.error e");
  check("SQL.execute db \"CREATE TABLE t (id INTEGER, value BLOB)\" [] == Ok ()");
  check("SQL.execute db \"INSERT INTO t VALUES (?, ?)\" [SqlInt 42, SqlBlob (String.of_codes [0,255])] == Ok ()");
  check("SQL.query db \"SELECT * FROM t\" [] == Ok [[(\"id\", SqlInt 42), (\"value\", SqlBlob (String.of_codes [0,255]))]]");
  check("match SQL.query db \"SELECT ?\" [] with | Err _ -> true | _ -> false");
  check("match SQL.query db \"SELECT 1; SELECT 2\" [] with | Err _ -> true | _ -> false");
  check("SQL.transaction db (fn connection -> match SQL.execute connection \"DELETE FROM t\" [] with | Ok _ -> Err \"abort\" | Err e -> Err e) == Err \"abort\"");
  check("SQL.query db \"SELECT count(*) AS n FROM t\" [] == Ok [[(\"n\", SqlInt 1)]]");
  eval("SQL.close db");
  check("match SQL.query db \"SELECT 1\" [] with | Err _ -> true | _ -> false");
  eval("val listener := match Nng.create \"pair\" with | Ok x -> x | Err e -> Core.error e");
  eval("val client := match Nng.create \"pair\" with | Ok x -> x | Err e -> Core.error e");
  check("Nng.listen listener \"inproc://metalib-test\" == Ok ()");
  check("Nng.dial client \"inproc://metalib-test\" == Ok ()");
  check("Nng.send client \"bytes\" == Ok ()");
  check("Nng.recv listener == Ok \"bytes\"");
  eval("Nng.close client"); eval("Nng.close listener");
  eval("val server := Webserver.create ()");
  check("Webserver.text server \"/hello\" \"world\" == Ok ()");
  eval("val port := match Webserver.start server \"127.0.0.1\" 0 with | Ok p -> p | Err e -> Core.error e");
  check("match Http.get (\"http://127.0.0.1:\" ^ String.of_int port ^ \"/hello\") with | Ok response -> response.status == 200 && response.body == \"world\" | Err _ -> false");
  eval("Webserver.stop server");
#endif
}
