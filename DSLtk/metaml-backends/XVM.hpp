#pragma once
#include "../metalib/xvm/XVM.hpp"

namespace metaml::backends {
// Full-language compiled backend. Compilation validates syntax; checking
// happens on execution in the destination Session, just as with Session::eval.
// The core executor lowers every expression kind to bytecode/native call blocks.
class XVM {
  metalib::xvm::VM vm_;
public:
  explicit XVM(metalib::arch::Target target = metalib::arch::host()) : vm_(target) {}
  RunReport eval(Session &session, std::string source, std::string file = "<xvm>",
                 bool jit = true, RunMode mode = RunMode::Eval)
  {
    auto parsed = parse(source, file);
    if (!parsed.ok()) { RunReport r; r.errors = parsed.diagnostics; return r; }
    return vm_.run(session, {std::move(source), std::move(file)}, jit, mode);
  }
};
}
