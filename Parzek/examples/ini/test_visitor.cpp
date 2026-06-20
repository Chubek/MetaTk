#include "ini-Parzek.hpp"
#include "IniVisitor.hpp"
#include <iostream>

struct IniPrinter : IniLike_parzek::IniVisitor {
  int depth = 0;
  
  void enter_node(const IniLike_parzek::ASTNode& node) override {
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "Enter: " << node.rule_name << " at " << node.line << ":" << node.column;
    if (!node.text.empty()) {
      std::cout << " [" << node.text << "]";
    }
    std::cout << "\n";
    ++depth;
  }
  
  void exit_node(const IniLike_parzek::ASTNode& node) override {
    --depth;
  }
  
  void visit_section(const IniLike_parzek::ASTNode& node) override {
    std::cout << "Found section!\n";
  }
  
  void visit_entry(const IniLike_parzek::ASTNode& node) override {
    std::cout << "Found entry!\n";
  }
};

int main() {
  auto txt = R"(
[database]
host = localhost
port = 5432
)";

  IniLike_parzek::Parser parser;
  auto ast = parser.parse_ast(txt);
  
  if (ast) {
    std::cout << "Parse successful!\n";
    IniPrinter printer;
    printer.visit(*ast);
  } else {
    std::cout << "Parse failed\n";
  }
  
  return 0;
}
