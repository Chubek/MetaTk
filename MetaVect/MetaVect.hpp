#ifndef METAVECT_HPP
#define METAVECT_HPP
#include "MetaVect.h"
#include <stdexcept>
#include <string>
namespace metavect {
class error : public std::runtime_error {
public:
    explicit error(const std::string& message) : std::runtime_error(message) {}
};
inline const char* version() noexcept { return mv_version_string(); }
inline void check(mv_status status) {
    if (status != MV_STATUS_OK) throw error(mv_status_string(status));
}
}
#endif
