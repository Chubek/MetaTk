#ifndef XBIND_QAMRPP_HPP
#define XBIND_QAMRPP_HPP

#include <string>
#include <vector>
#include <map>

namespace xbind::qamrpp {

using Options = std::map<std::string, std::string>;

struct Typedef {
  std::string c_type;
  std::string native_type;
  Options options;
};

struct Target {
  std::string name;
  Options options;
  std::vector<Typedef> typedefs;
  std::vector<std::string> features;
};

struct Param {
  std::string name;
  std::string c_type;
  Options options;
};

struct Function {
  std::string name;
  std::string returns;
  std::vector<Param> params;
  Options options;
  std::vector<std::string> features;
};

struct Manifest {
  std::string path;
  std::string name;
  std::string version;
  std::vector<std::string> headers;
  std::vector<std::string> stdlib_modules;
  std::vector<std::string> targets;
  std::vector<Function> functions;
  std::vector<Typedef> typedefs;
  std::vector<Target> target_options;
  std::vector<std::string> features;
};

Manifest load_spec_file(const std::string &path);
std::string dump_manifest(const Manifest &manifest);

} // namespace xbind::qamrpp

#endif /* XBIND_QAMRPP_HPP */
