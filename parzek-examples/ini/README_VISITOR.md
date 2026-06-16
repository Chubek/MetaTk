# Visitor Pattern Integration

The generated visitor allows you to traverse and act upon the parsed AST.

## Generated Components

When you compile with `--create-visitor=IniVisitor.hpp`, Parzek generates:

1. **ASTNode structure** (in parser header) with:
   - `rule_name`: the grammar rule that matched
   - `text`: captured text content
   - `children`: child nodes (currently requires manual population)
   - `line`, `column`: source location

2. **Visitor base class** with:
   - `visit(node)`: main traversal entry point
   - `enter_node(node)`, `exit_node(node)`: hooks for every node
   - `visit_<rule>(node)`: per-rule visit methods

## Example Usage

```cpp
#include "ini-Parzek.hpp"
#include "IniVisitor.hpp"
#include <iostream>

struct IniPrinter : IniLike_parzek::IniVisitor {
  int depth = 0;
  
  void enter_node(const IniLike_parzek::ASTNode& node) override {
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << node.rule_name << " [" << node.text << "]\n";
    ++depth;
  }
  
  void exit_node(const IniLike_parzek::ASTNode& node) override {
    --depth;
  }
  
  void visit_section(const IniLike_parzek::ASTNode& node) override {
    std::cout << "Processing section: " << node.text << "\n";
  }
};

int main() {
  IniLike_parzek::Parser parser;
  auto ast = parser.parse_ast(input);
  
  if (ast) {
    IniPrinter printer;
    printer.visit(*ast);
  }
}
```

## Current Limitations

The AST generation currently captures flat text results. To build a proper tree with children, you need to:

1. Manually populate `children` by calling child rule `_ast()` methods
2. Or extend the code generator to emit recursive AST building

## Integration Points

- Parser provides `parse_ast(input)` for AST generation
- Each syntactic rule gets a `parse_<rule>_ast()` method
- Visitor automatically traverses `children` vector
- Override `visit_<rule>()` methods for rule-specific actions
- Use `enter_node()`/`exit_node()` for generic pre/post processing

