#include "MetaML.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
struct Policy {
  std::unique_ptr<metaml::Session> active;
  unsigned generation=0;
  static bool decide(metaml::Session& s, int age) {
    // age is a trusted native integer rendered canonically, never raw request text.
    auto r=s.eval("allow " + std::to_string(age));
    if(r.is_err()) throw std::runtime_error(s.render(r.unwrap_err().front()));
    auto value=r.unwrap();
    if(value.kind!=metaml::VK::Bool) throw std::runtime_error("policy did not return Bool");
    return value.i!=0;
  }
  bool reload(const std::string& source) {
    auto candidate=std::make_unique<metaml::Session>();
    candidate->set_primitive_allowlist({});
    candidate->set_max_depth(1000);
    auto r=candidate->eval(source,"candidate.ml");
    if(r.is_err()) return false;
    auto t=candidate->type_of("allow 20");
    if(t.is_err() || t.unwrap()!="Bool") return false;
    try {
      if(decide(*candidate,17) || !decide(*candidate,18) || !decide(*candidate,20)) return false;
    } catch(const std::exception&) { return false; }
    active=std::move(candidate); ++generation; return true;
  }
  bool allow(int age) {
    if(!active) throw std::runtime_error("no active policy");
    return decide(*active,age);
  }
};
int main() {
  try {
    Policy p;
    if(!p.reload("val allow := fn age -> age >= 18")) return 1;
    if(p.reload("val allow := fn age -> true")) return 1;
    if(p.reload("val allow := fn age -> age + true")) return 1;
    if(p.generation!=1 || p.allow(17) || !p.allow(20)) return 1;
    std::cout << "generation=1; age17=deny; age20=allow; rejected candidates preserved active policy\n";
    return 0;
  } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
