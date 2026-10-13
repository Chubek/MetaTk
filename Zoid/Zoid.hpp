#pragma once

#include "DSLtk.hpp"

#include <vector>
#include <string>
#include <stack>
#include <set>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <memory>

namespace zoid
{

struct ZoidDSL
	: dsl::DSL<
	    ZoidDSL,
	    dsl::AST,
	    dsl::Rewrite,
	    dsl::Pipeline,
	    dsl::Operator
	  >
	{};

auto create_grammar(void)
{
    
}

};
