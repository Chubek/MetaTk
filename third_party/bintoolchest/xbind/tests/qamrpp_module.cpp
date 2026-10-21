#include <QaMRpp.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

std::string read_file(const char *path) {
  std::ifstream input(path, std::ios::in | std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

qamrpp::ValuePtr string_key(const char *key) {
  return std::make_shared<qamrpp::Value>(std::string(key));
}

std::string field(qamrpp::Context &ctx, const qamrpp::ValuePtr &table, const char *key) {
  qamrpp::ValuePtr value = ctx.table_raw_get(table, string_key(key));
  return value && value->type == qamrpp::Value::STRING ? value->string_value : std::string();
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: qamrpp_module <libqamrpp_xbind> <spec>\n";
    return 2;
  }

  qamrpp::Context ctx;
  if (!ctx.load_library(argv[1])) {
    std::cerr << "failed to load QaMRpp XBind module: " << argv[1] << "\n";
    return 1;
  }

  qamrpp::ValuePtr manifest = ctx.run(read_file(argv[2]));
  if (!manifest || manifest->type != qamrpp::Value::TABLE) {
    std::cerr << "spec did not return a table\n";
    return 1;
  }
  if (field(ctx, manifest, "kind") != "library" || field(ctx, manifest, "name") != "hello") {
    std::cerr << "unexpected manifest from QaMRpp module\n";
    return 1;
  }
  // The plugin must preserve annotations, typedefs and version, not rebuild a
  // lossy subset of the manifest with a separate set of constructors.
  auto probe = ctx.run(R"(
local x=require("lxbind")
local options={nullable=true,ownership="borrowed"}
local a=x.param("a","const char *",options)
local b=x.param("b","const char *",options)
return x.validate_library(x.library{name="probe",version="2.0",targets={x.target("Java",{package="stone.probe"})},
typedefs={x.typedef("status_t","int")},functions={x.func("f",{params={a,b}})}})
)");
  if (field(ctx, probe, "version") != "2.0") return 1;
  auto functions = ctx.table_raw_get(probe, string_key("functions"));
  auto function = ctx.table_raw_get(functions, std::make_shared<qamrpp::Value>(int64_t(1)));
  auto params = ctx.table_raw_get(function, string_key("params"));
  auto first = ctx.table_raw_get(params, std::make_shared<qamrpp::Value>(int64_t(1)));
  auto nullable = ctx.table_raw_get(first, string_key("nullable"));
  if (field(ctx, first, "name") != "a" || field(ctx, first, "ownership") != "borrowed" ||
      !nullable || nullable->type != qamrpp::Value::BOOL || !nullable->bool_value) return 1;
  try {
    ctx.run("local x=require('lxbind'); return x.validate_library(x.library{name='a',functions={x.func('f'),x.func('f')}})");
    std::cerr << "plugin accepted duplicate function\n";
    return 1;
  } catch (const std::exception& error) {
    if (std::string(error.what()).find("duplicate function") == std::string::npos) throw;
  }
  return 0;
}
