#include "DSLtk.hpp"
#include <charconv>
#include <iostream>

struct Person { std::string name; int age; };
int integer(const dsl::ASTNode& n) {
  if (!n.is_leaf() || n.tag() != "integer")
    throw std::invalid_argument("expected integer leaf");
  int value{};
  auto text = n.value();
  auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size())
    throw std::invalid_argument("invalid integer");
  return value;
}
bool predicate(const dsl::ASTNode& n, const Person& row) {
  if (n.is_leaf() && n.tag() == "true") return true;
  if (n.tag() == "and" && n.size() == 2)
    return predicate(n.child(0), row) && predicate(n.child(1), row);
  if (n.tag() == "age-ge" && n.size() == 1)
    return row.age >= integer(n.child(0));
  throw std::invalid_argument("invalid query predicate");
}
std::vector<Person> relation(const dsl::ASTNode& n,
                             const std::vector<Person>& people) {
  if (n.is_leaf() && n.tag() == "source" && n.value() == "people")
    return people;
  if (n.tag() == "filter" && n.size() == 2) {
    auto rows = relation(n.child(1), people);
    std::erase_if(rows, [&](const Person& row) { return !predicate(n.child(0), row); });
    return rows;
  }
  throw std::invalid_argument("invalid relation");
}
std::vector<std::string> execute(const dsl::ASTNode& query,
                                 const std::vector<Person>& people) {
  if (query.tag() != "project-name" || query.size() != 1)
    throw std::invalid_argument("expected name projection");
  std::vector<std::string> result;
  for (const auto& person : relation(query.child(0), people))
    result.push_back(person.name);
  return result;
}
int main() {
  auto tree = dsl::node<"project-name">(
      dsl::node<"filter">(
          dsl::node<"and">(dsl::leaf<"true">(""),
                           dsl::node<"age-ge">(dsl::leaf<"integer">(18))),
          dsl::leaf<"source">("people")));
  auto rules = dsl::rewrite_set(
      dsl::rule<"and-true">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "and" && n.size() == 2
                && n.child(0).is_leaf() && n.child(0).tag() == "true";
          },
          [](const dsl::ASTNode& n) { return n.child(1); }),
      dsl::rule<"filter-true">(
          [](const dsl::ASTNode& n) {
            return n.tag() == "filter" && n.size() == 2
                && n.child(0).is_leaf() && n.child(0).tag() == "true";
          },
          [](const dsl::ASTNode& n) { return n.child(1); }));
  const std::vector<Person> data{{"Ada", 36}, {"Lin", 17}, {"Grace", 85}};
  auto optimized = rules.apply(tree);
  auto before = execute(tree, data), after = execute(optimized, data);
  assert(before == after);
  assert((after == std::vector<std::string>{"Ada", "Grace"}));
  assert(rules.apply(optimized) == optimized);
  std::cout << tree.dump() << '\n' << optimized.dump() << '\n';
  for (const auto& name : after) std::cout << name << '\n';
}
