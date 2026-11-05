#pragma once
#include "../chunk/Chunk.hpp"
#include "../system/System.hpp"
#include "Executable.hpp"

namespace metalib::xvm {
class VM {
  arch::Target target_;
public:
  explicit VM(arch::Target target = arch::host()) : target_(target) {}
  metaml::RunReport run(metaml::Session &session, const Chunk &chunk, bool jit = false,
                        metaml::RunMode mode = metaml::RunMode::Eval)
  {
    validate(chunk);
    if (jit && (target_ == arch::Target::Unsupported || target_ != arch::host()))
      throw std::runtime_error("cannot JIT a non-native architecture");
    auto &engine = session.interp().engine;
    struct Restore { metaml::ExecutionEngine &slot, saved; ~Restore() { slot = saved; } } restore{engine, engine};
    engine = jit ? metaml::ExecutionEngine::NativeXVM : metaml::ExecutionEngine::PortableXVM;
    return session.run_source(chunk.source, chunk.file, mode);
  }
};
inline arch::Target target(const std::string &name)
{
  if (name == "amd64") return arch::Target::Amd64;
  if (name == "aarch64") return arch::Target::Aarch64;
  if (name == "riscv64") return arch::Target::Riscv64;
  throw std::runtime_error("unknown architecture");
}
}
namespace metalib {
inline void install_XVM(metaml::Session &s)
{
  using namespace support;
  install_Arch(s); install_Chunk(s);
  if (s.find_primitive("XVM.run")) return;
  s.declare("type XvmValue\n", "<metalib XVM>");
  for (bool jit : {false, true}) {
    prim(s, jit ? "XVM.jit" : "XVM.run", "String -> XvmChunk -> Result XvmValue String", 2,
      [jit](C &c, V &a) {
        auto chunk = handle<xvm::Chunk>(a[1], "XvmChunk", c).data;
        try {
          xvm::VM vm(xvm::target(S(a[0])));
          auto r = vm.run(c.session, chunk, jit);
          if (!r.ok()) return err(r.errors.front().headline());
          auto h = std::make_shared<Handle<Value>>("XvmValue");
          h->data = r.value.value_or(unit()); return ok(wrap_handle(h));
        } catch (const std::exception &e) { return err(e.what()); }
      });
  }
  prim(s, "XVM.show", "XvmValue -> String", 1, [](C &c, V &a) {
    return str(metaml::show_value(handle<Value>(a[0], "XvmValue", c).data));
  });
}
// Explicit opt-in: filesystem access and code execution are not part of the
// portable bundle. Installing does not grant any primitive capabilities.
inline void install_toolchain(metaml::Session &s)
{
  install_Absyn(s); install_System(s); install_Objfile(s); install_XVM(s);
}
}
