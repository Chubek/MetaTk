/** @file Metalib.h
 * Portable host-module bundle. Install before load_stdlib(). Installation
 * does not grant capabilities: select allowed names with the Session API.
 * POSIX File/Process and native CBridge/FFI remain explicit opt-in headers.
 * HTTP, Webserver, Fiber, Channels and Threading are not implemented.
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
inline void install_all(metaml::Session &s)
{
  if (s.find_primitive("Metalib.version")) return;
  install_String(s); install_List(s); install_Hash(s); install_IO(s);
  install_Bridge(s); install_Buffer(s); install_Table(s); install_URI(s);
  install_Graph(s); install_JSON(s); install_TOML(s); install_YAML(s);
  install_SExpr(s); install_ASTBuilder(s); install_WalkAST(s); install_WalkVisitor(s);
  install_VisitorBuilder(s); install_FlexBridge(s); install_Parsec(s); install_YACCBridge(s);
  install_Frontend(s); install_Type(s); install_Module(s); install_Loader(s);
  s.install_primitive("Metalib.version", "Unit -> String", 1,
    [](metaml::PrimContext &, std::vector<metaml::Value> &) {
      return metaml::make_str(metaml::version);
    });
}
}
