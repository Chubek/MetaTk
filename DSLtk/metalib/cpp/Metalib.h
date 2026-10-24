/** @file Metalib.h
 * Portable host-module bundle. Install before load_stdlib(). Installation
 * does not grant capabilities: select allowed names with the Session API.
 * POSIX File/Process and native CBridge/FFI remain explicit opt-in headers.
 * Network and concurrency modules are outside this portable bundle.
 */
#pragma once
#include "String.h"
#include "List.h"
#include "Hash.h"
#include "IO.h"
#include "Bridge.h"
#include "Buffer.h"
#include "Table.h"
#include "URI.h"
#include "Graph.h"
#include "JSON.h"
#include "TOML.h"
#include "YAML.h"
#include "SExpr.h"
#include "ASTBuilder.h"
#include "WalkAST.h"
#include "WalkVisitor.h"
#include "VisitorBuilder.h"
#include "FlexBridge.h"
#include "Parsec.h"
#include "YACCBridge.h"
#include "Frontend.h"
#include "Type.h"
#include "Module.h"
#include "Loader.h"

namespace metalib {
// Layers can be composed in any order; dependencies are installed first.
// Probe module entry points so composing layers preserves existing handles
// and primitive identities instead of redeclaring their opaque types.
inline void install_foundation(metaml::Session &s)
{
  if (!s.find_primitive("String.utf8_length")) install_String(s);
  if (!s.find_primitive("List.take_while")) install_List(s);
  if (!s.find_primitive("Hash.fnv1a")) install_Hash(s);
  if (!s.find_primitive("Bridge.kind")) install_Bridge(s);
  if (!s.find_primitive("Buffer.create")) install_Buffer(s);
  if (!s.find_primitive("Table.create")) install_Table(s);
  if (!s.find_primitive("Graph.nodes")) install_Graph(s);
}

inline void install_data(metaml::Session &s)
{
  install_foundation(s);
  if (!s.find_primitive("URI.parse")) install_URI(s);
  if (!s.find_primitive("JSON.parse")) install_JSON(s);
  if (!s.find_primitive("TOML.parse")) install_TOML(s);
  if (!s.find_primitive("YAML.parse")) install_YAML(s);
  if (!s.find_primitive("SExpr.parse")) install_SExpr(s);
}

inline void install_language(metaml::Session &s)
{
  install_data(s);
  if (!s.find_primitive("ASTBuilder.leaf")) install_ASTBuilder(s);
  if (!s.find_primitive("WalkAST.preorder")) install_WalkAST(s);
  if (!s.find_primitive("WalkVisitor.walk")) install_WalkVisitor(s);
  if (!s.find_primitive("VisitorBuilder.make")) install_VisitorBuilder(s);
  if (!s.find_primitive("FlexBridge.tokenize")) install_FlexBridge(s);
  if (!s.find_primitive("Parsec.char")) install_Parsec(s);
  if (!s.find_primitive("YACCBridge.check")) install_YACCBridge(s);
  if (!s.find_primitive("Frontend.tokens")) install_Frontend(s);
  if (!s.find_primitive("Type.normalize")) install_Type(s);
  if (!s.find_primitive("Module.names")) install_Module(s);
}

inline void install_io(metaml::Session &s)
{
  install_foundation(s);
  if (!s.find_primitive("IO.flush")) install_IO(s);
  if (!s.find_primitive("Loader.load")) install_Loader(s);
}

inline void install_all(metaml::Session &s)
{
  install_language(s);
  install_io(s);
  if (s.find_primitive("Metalib.version")) return;
  s.install_primitive("Metalib.version", "Unit -> String", 1,
    [](metaml::PrimContext &, std::vector<metaml::Value> &) {
      return metaml::make_str(metaml::version);
    });
}
}
