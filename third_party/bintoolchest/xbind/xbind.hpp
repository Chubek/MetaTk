#ifndef XBIND_HPP
#define XBIND_HPP

#include "xbind.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xbind {

using TargetInfo = xbind_target_info;

inline std::string_view version() noexcept { return xbind_version(); }

inline std::vector<TargetInfo> targets() {
  std::vector<TargetInfo> result;
  const auto count = xbind_target_count();
  result.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    result.push_back(*xbind_target_at(index));
  }
  return result;
}

inline TargetInfo target(std::string_view name) {
  std::string owned(name);
  const auto *info = xbind_find_target(owned.c_str());
  if (!info) {
    throw std::runtime_error("unknown XBind target: " + owned);
  }
  return *info;
}

inline std::string resolve_spec(std::string_view directory,
                                std::string_view library_name) {
  char output[4096];
  std::string directory_string(directory);
  std::string library_string(library_name);
  const auto status = xbind_resolve_spec(directory_string.c_str(),
                                         library_string.c_str(), output,
                                         sizeof(output));
  if (status != XBIND_STATUS_OK) {
    throw std::runtime_error("could not resolve XBind spec '" + library_string +
                             "': " + xbind_status_string(status));
  }
  return output;
}

} // namespace xbind

#endif /* XBIND_HPP */
