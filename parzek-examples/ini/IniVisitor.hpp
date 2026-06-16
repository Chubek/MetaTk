#pragma once

#include "ini-Parzek.hpp"

namespace IniLike_parzek {

struct IniVisitor {
  virtual ~IniVisitor() = default;
  virtual void visit(const ASTNode& node);
  virtual void enter_node(const ASTNode& node) {}
  virtual void exit_node(const ASTNode& node) {}
  virtual void visit_section(const ASTNode& node) {}
  virtual void visit_entry(const ASTNode& node) {}
  virtual void visit_config(const ASTNode& node) {}
};

void IniVisitor::visit(const ASTNode& node) {
  enter_node(node);
  if (node.rule_name == "section") {
    visit_section(node);
  }
  else if (node.rule_name == "entry") {
    visit_entry(node);
  }
  else if (node.rule_name == "config") {
    visit_config(node);
  }
  for (const auto& child : node.children) {
    if (child) visit(*child);
  }
  exit_node(node);
}

}
