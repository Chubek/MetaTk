#pragma once
#include "../ast/Absyn.hpp"
#include "../objfile/Objfile.hpp"

namespace metalib::xvm {
enum class Opcode : unsigned char { ExecuteProgram = 1, Halt = 0 };
// Version-1 source-module envelope, retained for portable persistence. Execution
// checks the destination bindings and lowers expressions to the core XVM IR;
// this envelope is deliberately not a dump of session-specific checked code.
struct Chunk {
  std::string source, file;
  std::vector<Opcode> code{Opcode::ExecuteProgram, Opcode::Halt};
};
inline Chunk compile(std::string source, std::string file = "<xvm>")
{
  auto parsed = metaml::parse(source, file);
  if (!parsed.ok()) throw std::runtime_error(parsed.diagnostics.front().headline());
  return {std::move(source), std::move(file)};
}
inline void validate(const Chunk &chunk)
{
  if (chunk.code != std::vector<Opcode>{Opcode::ExecuteProgram, Opcode::Halt})
    throw std::runtime_error("invalid XVM instruction stream");
}
// Versioned, architecture-independent wire format. Never serializes pointers or
// checked AST cells, which belong to the session in which they were checked.
inline std::string encode(const Chunk &chunk)
{
  validate(chunk);
  std::string out("XVM\0\x01\0\0\0", 8);
  arch::little(out, chunk.file.size(), 8); arch::little(out, chunk.source.size(), 8);
  out += '\x01'; out += '\0'; out += chunk.file; out += chunk.source;
  return out;
}
inline Chunk decode(const std::string &bytes)
{
  if (bytes.size() < 26 || bytes.compare(0, 8, std::string("XVM\0\x01\0\0\0", 8)))
    throw std::runtime_error("invalid XVM chunk header or version");
  auto file_size = objfile::get(bytes, 8, 8), source_size = objfile::get(bytes, 16, 8);
  objfile::range(bytes, 26, file_size);
  objfile::range(bytes, 26 + file_size, source_size);
  if (file_size + source_size != bytes.size() - 26 || bytes[24] != 1 || bytes[25] != 0)
    throw std::runtime_error("invalid XVM chunk lengths or opcodes");
  return compile(bytes.substr(26 + file_size), bytes.substr(26, file_size));
}
inline Chunk from_file(const std::string &path)
{
  auto bytes = absyn_detail::read_file(path);
  return bytes.compare(0, 4, std::string("XVM\0", 4)) == 0 ? decode(bytes) : compile(std::move(bytes), path);
}
}
namespace metalib {
inline void install_Chunk(metaml::Session &s)
{
  using namespace support;
  if (s.find_primitive("Chunk.of_source")) return;
  s.declare("type XvmChunk\n", "<metalib Chunk>");
  auto wrap = [](xvm::Chunk chunk) {
    auto h = std::make_shared<Handle<xvm::Chunk>>("XvmChunk");
    h->data = std::move(chunk); return wrap_handle(h);
  };
  prim(s, "Chunk.of_source", "String -> Result XvmChunk String", 1, [wrap](C &, V &a) {
    try { return ok(wrap(xvm::compile(S(a[0])))); }
    catch (const std::exception &e) { return err(e.what()); }
  });
  prim(s, "Chunk.of_file", "String -> Result XvmChunk String", 1, [wrap](C &, V &a) {
    try { return ok(wrap(xvm::from_file(S(a[0])))); }
    catch (const std::exception &e) { return err(e.what()); }
  });
  prim(s, "Chunk.current", "Unit -> Result XvmChunk String", 1, [wrap](C &c, V &) {
    auto p = c.session.current_program();
    return p ? ok(wrap(xvm::compile(p->source, p->file))) : err("no current program");
  });
  prim(s, "Chunk.encode", "XvmChunk -> String", 1, [](C &c, V &a) {
    return str(xvm::encode(handle<xvm::Chunk>(a[0], "XvmChunk", c).data));
  });
  prim(s, "Chunk.decode", "String -> Result XvmChunk String", 1, [wrap](C &, V &a) {
    try { return ok(wrap(xvm::decode(S(a[0])))); }
    catch (const std::exception &e) { return err(e.what()); }
  });
  prim(s, "Chunk.source", "XvmChunk -> String", 1, [](C &c, V &a) {
    return str(handle<xvm::Chunk>(a[0], "XvmChunk", c).data.source);
  });
  prim(s, "Chunk.filename", "XvmChunk -> String", 1, [](C &c, V &a) {
    return str(handle<xvm::Chunk>(a[0], "XvmChunk", c).data.file);
  });
}
}
