#include "DSLtk.hpp"
#include <charconv>
#include <iostream>

enum class Kind { Word, Number, Equal, Semi };
struct Token { Kind kind; std::string text; std::size_t offset; };
struct Settings {
  int port = 8080;
  std::string mode = "release";
};
std::vector<Token> tokenize(const std::string& source) {
  std::vector<Token> tokens;
  auto grammar = dsl::create_peg_definition();
  grammar.add_rule<"[ \t\r\n]+">().channel = dsl::PEGIgnoreChannel;
  auto emit = [&](Kind kind) {
    return [&, kind](dsl::PEGMatch& m) {
      tokens.push_back({kind, std::string(m.value), m.begin});
    };
  };
  grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*">(emit(Kind::Word));
  grammar.add_rule<"[0-9]+">(emit(Kind::Number));
  grammar.add_rule<"=">(emit(Kind::Equal));
  grammar.add_rule<";">(emit(Kind::Semi));
  auto result = grammar.parse(source);
  if (!result.ok()) throw std::invalid_argument(std::string(result.error_message()));
  return tokens;
}
Settings load(std::string source) {
  dsl::Preprocessor pp;
  source = pp.process(source);
  const auto tokens = tokenize(source);
  Settings candidate;
  std::unordered_set<std::string> defined;
  for (std::size_t i = 0; i < tokens.size(); i += 4) {
    if (tokens.size() - i < 4 || tokens[i].kind != Kind::Word
        || tokens[i+1].kind != Kind::Equal || tokens[i+3].kind != Kind::Semi)
      throw std::invalid_argument("expected key = value ;");
    const auto& key = tokens[i].text;
    const auto& value = tokens[i+2];
    if (!defined.insert(key).second) throw std::invalid_argument("duplicate key");
    if (key == "port") {
      int number{};
      auto [end, error] = std::from_chars(value.text.data(),
          value.text.data() + value.text.size(), number);
      if (value.kind != Kind::Number || error != std::errc{}
          || end != value.text.data() + value.text.size() || number < 1 || number > 65535)
        throw std::invalid_argument("invalid port");
      candidate.port = number;
    } else if (key == "mode") {
      if (value.kind != Kind::Word || (value.text != "debug" && value.text != "release"))
        throw std::invalid_argument("invalid mode");
      candidate.mode = value.text;
    } else throw std::invalid_argument("unknown key");
  }
  return candidate;
}
int main() {
  auto settings = load("#define PORT 9000\nport = PORT; mode = debug;\n");
  assert(settings.port == 9000 && settings.mode == "debug");
  for (auto bad : {"port=0;", "mode=unknown;", "port=80;port=81;",
                   "port=80", "other=1;", "port=@;"}) {
    bool rejected = false;
    try { (void)load(bad); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected);
  }
  std::cout << "port=" << settings.port << " mode=" << settings.mode << '\n';
}
