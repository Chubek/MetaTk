// Internal bridge shared by the CLI and the loadable QaMRpp library.
#pragma once
#include <QaMRpp.hpp>

namespace xbind::qamrpp {
::qamrpp::ValuePtr runtime_module(::qamrpp::Context& ctx);
}
