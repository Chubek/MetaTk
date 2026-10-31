from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
chapters=json.loads((root/'tools/content.json').read_text())
pre='#include "MetaML.hpp"\n#include <cassert>\n#include <iostream>\n#include <cstdint>\n'
allcpp=[pre,'#undef assert\n#define assert(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(#__VA_ARGS__); } while(0)\n']
runner=[]
for i,ch in enumerate(chapters,1):
 for j,x in enumerate(ch['lessons'],1):
  name=f'ex{i:02}_{j:02}'
  cpp=x['cpp']; d=x['d']
  (root/f'examples/cpp/{name}.cpp').write_text(pre+'int main() {\n'+cpp+'\n}\n')
  (root/f'examples/d/{name}.d').write_text(f'module {name};\nimport DSLtk;\nimport MetaML;\nvoid main() {{\n'+d+'\n}\n')
  (root/f'examples/cpp/{name}.inc').write_text(cpp+'\n')
  (root/f'examples/d/{name}.inc').write_text(d+'\n')
  if 'ml' in x:
   (root/f'examples/{name}.ml').write_text(x['ml']+'\n')
  allcpp.append(f'void {name}() {{\n{cpp}\n}}\n')
  runner.append(f'try {{ {name}(); ++passed; }} catch(const std::exception& e) {{ ++failed; std::cerr << "{name}: " << e.what() << "\\n"; }}')
allcpp.append('int main() { int passed=0, failed=0;\n'+'\n'.join(runner)+'\nstd::cout << passed << " passed, " << failed << " failed\\n"; return failed ? 1 : 0; }\n')
(root/'examples/all_cpp.cpp').write_text(''.join(allcpp))
