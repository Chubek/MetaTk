#pragma once
#include "MetaTeX.h"

namespace metatex {
int runRepl(Engine& engine,const std::string& format,const std::string& input,
            const std::string& output,const std::string& target);
}
