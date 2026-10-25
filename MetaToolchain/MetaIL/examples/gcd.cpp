#include "MetaIL.hpp"
#include <iostream>

int main() {
    try {
        metail::Context context;
        // Euclid's loop for nonnegative inputs. This example emits/analyzes IR;
        // it does not claim to compile or execute native code.
        const std::string source=R"(module euclid;
fn gcd(a, b) {
entry:
  goto loop;
loop:
  br b != 0, step, done;
step:
  let remainder = a % b;
  let a = b;
  let b = remainder;
  goto loop;
done:
  return a + 0;
})";
        const auto result=context.optimize(source,"AddZeroRight,ConstantFolding");
        std::cout<<result.source;
        std::cout<<context.analyze(result.source,"NaturalLoops,LiveVariables");
        std::cout<<result.stats;
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
