#pragma once
#include "../MetaML.hpp"

namespace metaml::backends {
// Full-language, host-native QBE backend. Requires METAML_ENABLE_QBE and the
// optional metaml-qbe compiler service; unavailable builds report M0409.
class QBE {
public:
  RunReport eval(Session &session, std::string source, std::string file = "<qbe>",
                 RunMode mode = RunMode::Eval)
  {
    auto &engine = session.interp().engine;
    struct Restore {
      ExecutionEngine &slot, saved;
      ~Restore() { slot = saved; }
    } restore{engine, engine};
    engine = ExecutionEngine::QBE;
    return session.run_source(source, file, mode);
  }
};
}
